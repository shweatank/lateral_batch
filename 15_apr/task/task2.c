#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "calc_irq"
#define KBD_IRQ 1
#define KBD_DATA_PORT 0x60

static struct kobject *calc_kobj;
static int input_number;
static int result;
static atomic_t result_ready = ATOMIC_INIT(0);

static struct work_struct calc_work;
static DECLARE_WAIT_QUEUE_HEAD(result_wq);

static int major;

static void calc_work_handler(struct work_struct *work)
{
    result = input_number * input_number;
    pr_info("%s: Calculated %d^2 = %d\n", DRIVER_NAME, input_number, result);
    atomic_set(&result_ready, 1);
    wake_up_interruptible(&result_wq);
}

static irqreturn_t kbd_irq_handler(int irq, void *dev_id)
{
    unsigned char scancode;

    scancode = inb(KBD_DATA_PORT);

    if (scancode & 0x80)
        return IRQ_HANDLED;

    pr_info("%s: Interrupt! Scheduling calculation for %d\n", DRIVER_NAME, input_number);
    schedule_work(&calc_work);

    return IRQ_HANDLED;
}

static ssize_t number_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", input_number);
}

static ssize_t number_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int ret;

    ret = kstrtoint(buf, 10, &input_number);
    if (ret)
        return ret;

    atomic_set(&result_ready, 0);
    pr_info("%s: Input number set to %d\n", DRIVER_NAME, input_number);
    return count;
}

static ssize_t result_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", result);
}

static struct kobj_attribute number_attr = __ATTR(number, 0664, number_show, number_store);
static struct kobj_attribute result_attr = __ATTR(result, 0444, result_show, NULL);

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    char msg[64];
    int msg_len, ret;

    pr_info("%s: Read called, waiting for result...\n", DRIVER_NAME);

    ret = wait_event_interruptible(result_wq, atomic_read(&result_ready) != 0);
    if (ret)
        return -ERESTARTSYS;

    atomic_set(&result_ready, 0);

    msg_len = snprintf(msg, sizeof(msg), "%d^2 = %d\n", input_number, result);

    if (len < msg_len)
        return -EINVAL;

    if (copy_to_user(buf, msg, msg_len))
        return -EFAULT;

    return msg_len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
};

static int __init calc_irq_init(void)
{
    int ret;

    calc_kobj = kobject_create_and_add("calc_irq", kernel_kobj);
    if (!calc_kobj)
        return -ENOMEM;

    ret = sysfs_create_file(calc_kobj, &number_attr.attr);
    if (ret) {
        kobject_put(calc_kobj);
        return ret;
    }

    ret = sysfs_create_file(calc_kobj, &result_attr.attr);
    if (ret) {
        sysfs_remove_file(calc_kobj, &number_attr.attr);
        kobject_put(calc_kobj);
        return ret;
    }

    INIT_WORK(&calc_work, calc_work_handler);

    major = register_chrdev(0, DRIVER_NAME, &fops);
    if (major < 0) {
        pr_err("%s: Failed to register char device\n", DRIVER_NAME);
        sysfs_remove_file(calc_kobj, &result_attr.attr);
        sysfs_remove_file(calc_kobj, &number_attr.attr);
        kobject_put(calc_kobj);
        return major;
    }

    ret = request_irq(KBD_IRQ, kbd_irq_handler, IRQF_SHARED, DRIVER_NAME, (void *)kbd_irq_handler);
    if (ret) {
        pr_err("%s: Failed to request IRQ %d\n", DRIVER_NAME, KBD_IRQ);
        unregister_chrdev(major, DRIVER_NAME);
        sysfs_remove_file(calc_kobj, &result_attr.attr);
        sysfs_remove_file(calc_kobj, &number_attr.attr);
        kobject_put(calc_kobj);
        return ret;
    }

    pr_info("%s: Module loaded. Major=%d\n", DRIVER_NAME, major);
    return 0;
}

static void __exit calc_irq_exit(void)
{
    free_irq(KBD_IRQ, (void *)kbd_irq_handler);
    cancel_work_sync(&calc_work);
    unregister_chrdev(major, DRIVER_NAME);
    sysfs_remove_file(calc_kobj, &result_attr.attr);
    sysfs_remove_file(calc_kobj, &number_attr.attr);
    kobject_put(calc_kobj);
    pr_info("%s: Module unloaded\n", DRIVER_NAME);
}

module_init(calc_irq_init);
module_exit(calc_irq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Sysfs Calculator with Interrupt-triggered Workqueue");
