#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define DEVICE_NAME "uart0"
#define CLASS_NAME  "uart_class"
#define UART_FIFO_SIZE 1024

#define UART_IOC_MAGIC      'U'
#define UART_IOC_SET_BAUD   _IOW(UART_IOC_MAGIC, 1, unsigned int)
#define UART_IOC_GET_BAUD   _IOR(UART_IOC_MAGIC, 2, unsigned int)
#define UART_IOC_FLUSH      _IO(UART_IOC_MAGIC,  3)

static dev_t dev_num;
static struct cdev uart_cdev;
static struct class *uart_class;

struct uart_dev {
    char            *fifo;
    unsigned int     head;
    unsigned int     tail;
    unsigned int     count;
    unsigned int     baud;
    struct mutex     lock;
    wait_queue_head_t rq;
};

static struct uart_dev *udev;

static bool fifo_empty(struct uart_dev *d) { return d->count == 0; }
static bool fifo_full(struct uart_dev *d)  { return d->count == UART_FIFO_SIZE; }

static int uart_open(struct inode *inode, struct file *file)
{
    pr_info("uart: device opened\n");
    return 0;
}

static int uart_release(struct inode *inode, struct file *file)
{
    pr_info("uart: device closed\n");
    return 0;
}

static ssize_t uart_read(struct file *file, char __user *buf,
                         size_t len, loff_t *ppos)
{
    size_t i = 0;
    char ch;

    if (mutex_lock_interruptible(&udev->lock))
        return -ERESTARTSYS;

    while (fifo_empty(udev)) {
        mutex_unlock(&udev->lock);

        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        if (wait_event_interruptible(udev->rq, !fifo_empty(udev)))
            return -ERESTARTSYS;

        if (mutex_lock_interruptible(&udev->lock))
            return -ERESTARTSYS;
    }

    while (i < len && !fifo_empty(udev)) {
        ch = udev->fifo[udev->tail];
        udev->tail = (udev->tail + 1) % UART_FIFO_SIZE;
        udev->count--;

        if (copy_to_user(buf + i, &ch, 1)) {
            mutex_unlock(&udev->lock);
            return -EFAULT;
        }
        i++;
    }

    mutex_unlock(&udev->lock);
    wake_up_interruptible(&udev->rq);
    return i;
}

static ssize_t uart_write(struct file *file, const char __user *buf,
                          size_t len, loff_t *ppos)
{
    size_t i = 0;
    char ch;

    if (mutex_lock_interruptible(&udev->lock))
        return -ERESTARTSYS;

    while (i < len && !fifo_full(udev)) {
        if (copy_from_user(&ch, buf + i, 1)) {
            mutex_unlock(&udev->lock);
            return -EFAULT;
        }
        udev->fifo[udev->head] = ch;
        udev->head = (udev->head + 1) % UART_FIFO_SIZE;
        udev->count++;
        i++;
    }

    mutex_unlock(&udev->lock);
    wake_up_interruptible(&udev->rq);

    if (i == 0)
        return -ENOSPC;

    return i;
}

static long uart_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned int val;

    switch (cmd) {
    case UART_IOC_SET_BAUD:
        if (copy_from_user(&val, (unsigned int __user *)arg, sizeof(val)))
            return -EFAULT;
        mutex_lock(&udev->lock);
        udev->baud = val;
        mutex_unlock(&udev->lock);
        pr_info("uart: baud set to %u\n", val);
        break;

    case UART_IOC_GET_BAUD:
        mutex_lock(&udev->lock);
        val = udev->baud;
        mutex_unlock(&udev->lock);
        if (copy_to_user((unsigned int __user *)arg, &val, sizeof(val)))
            return -EFAULT;
        break;

    case UART_IOC_FLUSH:
        mutex_lock(&udev->lock);
        udev->head = udev->tail = udev->count = 0;
        mutex_unlock(&udev->lock);
        pr_info("uart: fifo flushed\n");
        break;

    default:
        return -ENOTTY;
    }

    return 0;
}

static const struct file_operations uart_fops = {
    .owner          = THIS_MODULE,
    .open           = uart_open,
    .release        = uart_release,
    .read           = uart_read,
    .write          = uart_write,
    .unlocked_ioctl = uart_ioctl,
};

static int __init uart_init(void)
{
    int ret;

    udev = kzalloc(sizeof(*udev), GFP_KERNEL);
    if (!udev)
        return -ENOMEM;

    udev->fifo = kzalloc(UART_FIFO_SIZE, GFP_KERNEL);
    if (!udev->fifo) {
        ret = -ENOMEM;
        goto err_free_dev;
    }

    udev->baud = 115200;
    mutex_init(&udev->lock);
    init_waitqueue_head(&udev->rq);

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("uart: failed to allocate chrdev region\n");
        goto err_free_fifo;
    }

    cdev_init(&uart_cdev, &uart_fops);
    uart_cdev.owner = THIS_MODULE;

    ret = cdev_add(&uart_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("uart: failed to add cdev\n");
        goto err_unreg;
    }

    uart_class = class_create(CLASS_NAME);
    if (IS_ERR(uart_class)) {
        ret = PTR_ERR(uart_class);
        goto err_del_cdev;
    }

    device_create(uart_class, NULL, dev_num, NULL, DEVICE_NAME);

    pr_info("uart: loaded, major=%d minor=%d (loopback, baud=%u)\n",
            MAJOR(dev_num), MINOR(dev_num), udev->baud);
    return 0;

err_del_cdev:
    cdev_del(&uart_cdev);
err_unreg:
    unregister_chrdev_region(dev_num, 1);
err_free_fifo:
    kfree(udev->fifo);
err_free_dev:
    kfree(udev);
    return ret;
}

static void __exit uart_exit(void)
{
    device_destroy(uart_class, dev_num);
    class_destroy(uart_class);
    cdev_del(&uart_cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(udev->fifo);
    kfree(udev);
    pr_info("uart: unloaded\n");
}

module_init(uart_init);
module_exit(uart_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("uart sample");
MODULE_DESCRIPTION("Sample UART loopback char driver");
