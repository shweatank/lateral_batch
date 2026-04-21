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

#define DRIVER_NAME "sysfs_wq"
#define KBD_IRQ 1
#define KBD_DATA_PORT 0x60

static struct kobject *sysfs_kobj;
static int stored_value;
static int updated_value;
static atomic_t data_ready = ATOMIC_INIT(0);

static struct work_struct my_work;
static DECLARE_WAIT_QUEUE_HEAD(my_wq);

static int major;

static void work_handler(struct work_struct *work)
{
    updated_value = stored_value * 2;
    pr_info("%s: Workqueue updated value: %d -> %d\n", DRIVER_NAME, stored_value, updated_value);
    atomic_set(&data_ready, 1);
    wake_up_interruptible(&my_wq);
}

static irqreturn_t kbd_irq_handler(int irq, void *dev_id)
{
    unsigned char scancode;

    scancode = inb(KBD_DATA_PORT);

    if (scancode & 0x80)
        return IRQ_HANDLED;

    pr_info("%s: Interrupt received (scancode=0x%x), scheduling work\n", DRIVER_NAME, scancode);
    schedule_work(&my_work);

    return IRQ_HANDLED;
}

static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", updated_value);
}

static ssize_t value_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int ret;

    ret = kstrtoint(buf, 10, &stored_value);
    if (ret)
        return ret;

    pr_info("%s: Stored value set to %d\n", DRIVER_NAME, stored_value);
    return count;
}

static struct kobj_attribute value_attr = __ATTR(value, 0664, value_show, value_store);

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    char msg[64];
    int msg_len, ret;

    pr_info("%s: Read called, waiting for interrupt...\n", DRIVER_NAME);

    ret = wait_event_interruptible(my_wq, atomic_read(&data_ready) != 0);
    if (ret)
        return -ERESTARTSYS;

    atomic_set(&data_ready, 0);

    msg_len = snprintf(msg, sizeof(msg), "Updated value: %d\n", updated_value);

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

static int __init sysfs_wq_init(void)
{
    int ret;

    sysfs_kobj = kobject_create_and_add("sysfs_wq", kernel_kobj);
    if (!sysfs_kobj)
        return -ENOMEM;

    ret = sysfs_create_file(sysfs_kobj, &value_attr.attr);
    if (ret) {
        kobject_put(sysfs_kobj);
        return ret;
    }

    INIT_WORK(&my_work, work_handler);

    major = register_chrdev(0, DRIVER_NAME, &fops);
    if (major < 0) {
        pr_err("%s: Failed to register char device\n", DRIVER_NAME);
        sysfs_remove_file(sysfs_kobj, &value_attr.attr);
        kobject_put(sysfs_kobj);
        return major;
    }

    ret = request_irq(KBD_IRQ, kbd_irq_handler, IRQF_SHARED, DRIVER_NAME, (void *)kbd_irq_handler);
    if (ret) {
        pr_err("%s: Failed to request IRQ %d\n", DRIVER_NAME, KBD_IRQ);
        unregister_chrdev(major, DRIVER_NAME);
        sysfs_remove_file(sysfs_kobj, &value_attr.attr);
        kobject_put(sysfs_kobj);
        return ret;
    }

    pr_info("%s: Module loaded. Major=%d\n", DRIVER_NAME, major);
    return 0;
}

static void __exit sysfs_wq_exit(void)
{
    free_irq(KBD_IRQ, (void *)kbd_irq_handler);
    cancel_work_sync(&my_work);
    unregister_chrdev(major, DRIVER_NAME);
    sysfs_remove_file(sysfs_kobj, &value_attr.attr);
    kobject_put(sysfs_kobj);
    pr_info("%s: Module unloaded\n", DRIVER_NAME);
}

module_init(sysfs_wq_init);
module_exit(sysfs_wq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Sysfs + Workqueue + Waitqueue on Interrupt");
