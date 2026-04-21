#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/cdev.h>


#define DRIVER_NAME     "gpio_led_driver"
#define DEVICE_NAME     "gpio_led"
#define CLASS_NAME      "led"

#define BCM_LED         17
#define BCM_BTN         27
#define GPIO_LED        (BCM_LED + 512)
#define GPIO_BTN        (BCM_BTN + 512)

/* Reject button bounces closer than this many milliseconds apart */
#define DEBOUNCE_MS     200UL


static int            major;
static struct class  *led_class;
static struct device *led_device;
static struct cdev    led_cdev;

static unsigned int   irq_number;
static bool           led_state;
static spinlock_t     led_lock;
static unsigned long  last_irq_jiffies;

static atomic_t       open_count = ATOMIC_INIT(0);


static inline void led_apply(bool state)
{
    gpio_set_value(GPIO_LED, state ? 1 : 0);
}

static int led_open(struct inode *inodep, struct file *filep)
{
    if (!atomic_add_unless(&open_count, 1, 1))
        return -EBUSY;

    pr_info("%s: opened by PID %d\n", DRIVER_NAME, task_pid_nr(current));
    return 0;
}

static int led_release(struct inode *inodep, struct file *filep)
{
    atomic_dec(&open_count);
    pr_info("%s: closed\n", DRIVER_NAME);
    return 0;
}

static ssize_t led_write(struct file *filep, const char __user *buf,
                         size_t len, loff_t *offset)
{
    char cmd;
    unsigned long flags;

    if (len == 0)
        return 0;

    if (copy_from_user(&cmd, buf, 1))
        return -EFAULT;

    spin_lock_irqsave(&led_lock, flags);

    switch (cmd) {
    case '1':
        led_state = true;
        led_apply(led_state);
        pr_info("%s: LED → ON (write)\n", DRIVER_NAME);
        break;

    case '0':
        led_state = false;
        led_apply(led_state);
        pr_info("%s: LED → OFF (write)\n", DRIVER_NAME);
        break;

    case 't':
    case 'T':
        led_state = !led_state;
        led_apply(led_state);
        pr_info("%s: LED → %s (toggle via write)\n",
                DRIVER_NAME, led_state ? "ON" : "OFF");
        break;

    case '\n':
    case '\r':
        break;

    default:
        spin_unlock_irqrestore(&led_lock, flags);
        pr_warn("%s: unknown command '%c' (0x%02x). Use 0/1/t.\n",
                DRIVER_NAME, cmd, (unsigned char)cmd);
        return -EINVAL;
    }

    spin_unlock_irqrestore(&led_lock, flags);
    return (ssize_t)len;   /* consume entire buffer so callers don't retry */
}

static ssize_t led_read(struct file *filep, char __user *buf,
                        size_t len, loff_t *offset)
{
    char msg[3];
    unsigned long flags;
    size_t msg_len;

    if (*offset > 0)
        return 0;

    spin_lock_irqsave(&led_lock, flags);
    msg[0] = led_state ? '1' : '0';
    spin_unlock_irqrestore(&led_lock, flags);

    msg[1] = '\n';
    msg[2] = '\0';
    msg_len = 2;

    if (len < msg_len)
        return -EINVAL;

    if (copy_to_user(buf, msg, msg_len))
        return -EFAULT;

    *offset += msg_len;
    return (ssize_t)msg_len;
}

static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .release = led_release,
    .write   = led_write,
    .read    = led_read,
};


static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
    unsigned long flags;
    unsigned long now = jiffies;

    if (now - last_irq_jiffies < msecs_to_jiffies(DEBOUNCE_MS))
        return IRQ_HANDLED;

    if (gpio_get_value(GPIO_BTN) != 0)
        return IRQ_HANDLED;

    last_irq_jiffies = now;

    spin_lock_irqsave(&led_lock, flags);
    led_state = !led_state;
    led_apply(led_state);
    pr_info("%s: button pressed → LED %s\n",
            DRIVER_NAME, led_state ? "ON" : "OFF");
    spin_unlock_irqrestore(&led_lock, flags);

    return IRQ_HANDLED;
}


static ssize_t state_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
    unsigned long flags;
    bool s;

    spin_lock_irqsave(&led_lock, flags);
    s = led_state;
    spin_unlock_irqrestore(&led_lock, flags);

    return sysfs_emit(buf, "%d\n", s ? 1 : 0);
}
static DEVICE_ATTR_RO(state);


static int __init led_init(void)
{
    int ret;
    dev_t devno;

    pr_info("%s: loading\n", DRIVER_NAME);

    spin_lock_init(&led_lock);

    if (!gpio_is_valid(GPIO_LED)) {
        pr_err("%s: GPIO %d (LED) is not valid\n", DRIVER_NAME, GPIO_LED);
        return -ENODEV;
    }

    ret = gpio_request(GPIO_LED, DRIVER_NAME);
    if (ret) {
        pr_err("%s: cannot request GPIO %d (LED): %d\n",
               DRIVER_NAME, GPIO_LED, ret);
        return ret;
    }

    ret = gpio_direction_output(GPIO_LED, 0);
    if (ret) {
        pr_err("%s: cannot set GPIO %d as output: %d\n",
               DRIVER_NAME, GPIO_LED, ret);
        goto err_free_led_gpio;
    }

    if (!gpio_is_valid(GPIO_BTN)) {
        pr_err("%s: GPIO %d (button) is not valid\n", DRIVER_NAME, GPIO_BTN);
        ret = -ENODEV;
        goto err_free_led_gpio;
    }

    ret = gpio_request(GPIO_BTN, DRIVER_NAME);
    if (ret) {
        pr_err("%s: cannot request GPIO %d (button): %d\n",
               DRIVER_NAME, GPIO_BTN, ret);
        goto err_free_led_gpio;
    }

    ret = gpio_direction_input(GPIO_BTN);
    if (ret) {
        pr_err("%s: cannot set GPIO %d as input: %d\n",
               DRIVER_NAME, GPIO_BTN, ret);
        goto err_free_btn_gpio;
    }

    ret = gpio_to_irq(GPIO_BTN);
    if (ret < 0) {
        pr_err("%s: cannot map GPIO %d to IRQ: %d\n",
               DRIVER_NAME, GPIO_BTN, ret);
        goto err_free_btn_gpio;
    }
    irq_number = (unsigned int)ret;

    ret = request_irq(irq_number,
                      button_irq_handler,
                      IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                      "gpio_btn_irq",
                      (void *)&led_state);
    if (ret) {
        pr_err("%s: cannot request IRQ %u: %d\n",
               DRIVER_NAME, irq_number, ret);
        goto err_free_btn_gpio;
    }

    ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
    if (ret) {
        pr_err("%s: alloc_chrdev_region failed: %d\n", DRIVER_NAME, ret);
        goto err_free_irq;
    }
    major = MAJOR(devno);

    cdev_init(&led_cdev, &fops);
    led_cdev.owner = THIS_MODULE;

    ret = cdev_add(&led_cdev, devno, 1);
    if (ret) {
        pr_err("%s: cdev_add failed: %d\n", DRIVER_NAME, ret);
        goto err_unreg_chrdev;
    }

    led_class = class_create(CLASS_NAME);
    if (IS_ERR(led_class)) {
        ret = PTR_ERR(led_class);
        pr_err("%s: class_create failed: %d\n", DRIVER_NAME, ret);
        goto err_del_cdev;
    }

    led_device = device_create(led_class, NULL,
                               MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(led_device)) {
        ret = PTR_ERR(led_device);
        pr_err("%s: device_create failed: %d\n", DRIVER_NAME, ret);
        goto err_destroy_class;
    }

    ret = device_create_file(led_device, &dev_attr_state);
    if (ret) {
        pr_err("%s: device_create_file (state) failed: %d\n",
               DRIVER_NAME, ret);
        goto err_destroy_device;
    }

    pr_info("%s: loaded — /dev/%s  major=%d  IRQ=%u\n",
            DRIVER_NAME, DEVICE_NAME, major, irq_number);
    return 0;

err_destroy_device:
    device_destroy(led_class, MKDEV(major, 0));
err_destroy_class:
    class_destroy(led_class);
err_del_cdev:
    cdev_del(&led_cdev);
err_unreg_chrdev:
    unregister_chrdev_region(MKDEV(major, 0), 1);
err_free_irq:
    free_irq(irq_number, (void *)&led_state);
err_free_btn_gpio:
    gpio_free(GPIO_BTN);
err_free_led_gpio:
    gpio_free(GPIO_LED);
    return ret;
}


static void __exit led_exit(void)
{
    free_irq(irq_number, (void *)&led_state);
    gpio_free(GPIO_BTN);

    gpio_set_value(GPIO_LED, 0);
    gpio_free(GPIO_LED);

    device_remove_file(led_device, &dev_attr_state);
    device_destroy(led_class, MKDEV(major, 0));
    class_destroy(led_class);
    cdev_del(&led_cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);

    pr_info("%s: unloaded\n", DRIVER_NAME);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("GPIO LED driver with debounced button interrupt");
