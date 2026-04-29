#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define IR_GPIO_PIN  529

#define IR_IOC_MAGIC   'i'
#define IR_GET_STATUS  _IOR(IR_IOC_MAGIC, 0, int)

static int ir_irq;
static int ir_status = 0;   // updated ONLY by interrupt

static dev_t devno;
static struct cdev ir_cdev;
static struct class *ir_class;
static struct device *ir_device;

static irqreturn_t ir_irq_handler(int irq, void *dev_id)
{
    int value = gpio_get_value(IR_GPIO_PIN);

    ir_status = (value == 0) ? 1 : 0;

    pr_info("Val=%d\n",ir_status);
    return IRQ_HANDLED;
}

static int ir_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "IR SENSOR: Device opened\n");
    return 0;
}

static int ir_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "IR SENSOR: Device closed\n");
    return 0;
}

static long ir_ioctl(struct file *file,
                     unsigned int cmd,
                     unsigned long arg)
{
    int status;

    switch (cmd) {
    case IR_GET_STATUS:
        status = ir_status;
    	pr_info("Val=%d\n",ir_status);

        if (copy_to_user((int __user *)arg, &status, sizeof(status)))
            return -EFAULT;
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static struct file_operations ir_fops = {
    .owner          = THIS_MODULE,
    .open           = ir_open,
    .release        = ir_release,
    .unlocked_ioctl = ir_ioctl,
};


static int __init ir_driver_init(void)
{
    int ret;

    /* Allocate major/minor */
    ret = alloc_chrdev_region(&devno, 0, 1, "ir_sensor");
    if (ret)
        return ret;

    /* Init cdev */
    cdev_init(&ir_cdev, &ir_fops);
    ret = cdev_add(&ir_cdev, devno, 1);
    if (ret)
        goto err_chrdev;

    /* Create class */
    ir_class = class_create("ir_class");
    if (IS_ERR(ir_class)) {
        ret = PTR_ERR(ir_class);
        goto err_cdev;
    }

    /* Create device node: /dev/ir_sensor */
    ir_device = device_create(ir_class, NULL, devno, NULL, "ir_sensor");
    if (IS_ERR(ir_device)) {
        ret = PTR_ERR(ir_device);
        goto err_class;
    }

    /* GPIO setup */
    ret = gpio_request(IR_GPIO_PIN, "ir_gpio");
    if (ret)
        goto err_device;

    gpio_direction_input(IR_GPIO_PIN);

    ir_irq = gpio_to_irq(IR_GPIO_PIN);
    if (ir_irq < 0) {
        ret = ir_irq;
        goto err_gpio;
    }

    /* Request IRQ on BOTH edges */
    ret = request_irq(ir_irq,
                      ir_irq_handler,
                      IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                      "ir_irq",
                      NULL);
    if (ret)
        goto err_gpio;

    pr_info("IR driver loaded → /dev/ir_sensor created\n");
    return 0;

err_gpio:
    gpio_free(IR_GPIO_PIN);
err_device:
    device_destroy(ir_class, devno);
err_class:
    class_destroy(ir_class);
err_cdev:
    cdev_del(&ir_cdev);
err_chrdev:
    unregister_chrdev_region(devno, 1);
    return ret;
}

static void __exit ir_driver_exit(void)
{
    free_irq(ir_irq, NULL);
    gpio_free(IR_GPIO_PIN);
    device_destroy(ir_class, devno);
    class_destroy(ir_class);
    cdev_del(&ir_cdev);
    unregister_chrdev_region(devno, 1);
    pr_info("IR driver unloaded\n");
}

module_init(ir_driver_init);
module_exit(ir_driver_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IR Sensor Edge IRQ GPIO Driver");
