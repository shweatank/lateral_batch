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
#include <linux/string.h>

#define DRIVER_NAME "sysfs_io"
#define KBD_IRQ 1
#define KBD_DATA_PORT 0x60
#define BUF_SIZE 256

static struct kobject *sysfs_kobj;
static char input_buf[BUF_SIZE];
static char output_buf[BUF_SIZE];
static atomic_t output_ready = ATOMIC_INIT(0);

static struct work_struct process_work;
static DECLARE_WAIT_QUEUE_HEAD(output_wq);

static int major;

static void process_work_handler(struct work_struct *work)
{
    int len, i, j;
    char temp;

    strncpy(output_buf, input_buf, BUF_SIZE - 1);
    output_buf[BUF_SIZE - 1] = '\0';

    len = strlen(output_buf);
    for (i = 0, j = len - 1; i < j; i++, j--) {
        temp = output_buf[i];
        output_buf[i] = output_buf[j];
        output_buf[j] = temp;
    }

    pr_info("%s: Processed input '%s' -> output '%s'\n", DRIVER_NAME, input_buf, output_buf);
    atomic_set(&output_ready, 1);
    wake_up_interruptible(&output_wq);
}

static irqreturn_t kbd_irq_handler(int irq, void *dev_id)
{
    unsigned char scancode;

    scancode = inb(KBD_DATA_PORT);

    if (scancode & 0x80)
        return IRQ_HANDLED;

    if (input_buf[0] != '\0') {
        pr_info("%s: Interrupt! Scheduling processing\n", DRIVER_NAME);
        schedule_work(&process_work);
    }

    return IRQ_HANDLED;
}

static ssize_t input_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", input_buf);
}

static ssize_t input_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    size_t len;

    if (count >= BUF_SIZE)
        return -EINVAL;

    len = min(count, (size_t)(BUF_SIZE - 1));
    strncpy(input_buf, buf, len);
    input_buf[len] = '\0';

    if (len > 0 && input_buf[len - 1] == '\n')
        input_buf[len - 1] = '\0';

    atomic_set(&output_ready, 0);
    pr_info("%s: Input set to '%s'\n", DRIVER_NAME, input_buf);
    return count;
}

static ssize_t output_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", output_buf);
}

static struct kobj_attribute input_attr = __ATTR(input, 0664, input_show, input_store);
static struct kobj_attribute output_attr = __ATTR(output, 0444, output_show, NULL);

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    char msg[BUF_SIZE + 32];
    int msg_len, ret;

    pr_info("%s: Read called, waiting for processed output...\n", DRIVER_NAME);

    ret = wait_event_interruptible(output_wq, atomic_read(&output_ready) != 0);
    if (ret)
        return -ERESTARTSYS;

    atomic_set(&output_ready, 0);

    msg_len = snprintf(msg, sizeof(msg), "Output: %s\n", output_buf);

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

static int __init sysfs_io_init(void)
{
    int ret;

    sysfs_kobj = kobject_create_and_add("sysfs_io", kernel_kobj);
    if (!sysfs_kobj)
        return -ENOMEM;

    ret = sysfs_create_file(sysfs_kobj, &input_attr.attr);
    if (ret) {
        kobject_put(sysfs_kobj);
        return ret;
    }

    ret = sysfs_create_file(sysfs_kobj, &output_attr.attr);
    if (ret) {
        sysfs_remove_file(sysfs_kobj, &input_attr.attr);
        kobject_put(sysfs_kobj);
        return ret;
    }

    INIT_WORK(&process_work, process_work_handler);

    major = register_chrdev(0, DRIVER_NAME, &fops);
    if (major < 0) {
        pr_err("%s: Failed to register char device\n", DRIVER_NAME);
        sysfs_remove_file(sysfs_kobj, &output_attr.attr);
        sysfs_remove_file(sysfs_kobj, &input_attr.attr);
        kobject_put(sysfs_kobj);
        return major;
    }

    ret = request_irq(KBD_IRQ, kbd_irq_handler, IRQF_SHARED, DRIVER_NAME, (void *)kbd_irq_handler);
    if (ret) {
        pr_err("%s: Failed to request IRQ %d\n", DRIVER_NAME, KBD_IRQ);
        unregister_chrdev(major, DRIVER_NAME);
        sysfs_remove_file(sysfs_kobj, &output_attr.attr);
        sysfs_remove_file(sysfs_kobj, &input_attr.attr);
        kobject_put(sysfs_kobj);
        return ret;
    }

    pr_info("%s: Module loaded. Major=%d\n", DRIVER_NAME, major);
    return 0;
}

static void __exit sysfs_io_exit(void)
{
    free_irq(KBD_IRQ, (void *)kbd_irq_handler);
    cancel_work_sync(&process_work);
    unregister_chrdev(major, DRIVER_NAME);
    sysfs_remove_file(sysfs_kobj, &output_attr.attr);
    sysfs_remove_file(sysfs_kobj, &input_attr.attr);
    kobject_put(sysfs_kobj);
    pr_info("%s: Module unloaded\n", DRIVER_NAME);
}

module_init(sysfs_io_init);
module_exit(sysfs_io_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Sysfs Input/Output with Interrupt-triggered Workqueue Processing");
