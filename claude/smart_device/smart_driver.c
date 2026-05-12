#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include "smart_ioctl.h"

/* Module Metadata */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Senior Linux Developer");
MODULE_DESCRIPTION("Complete Character Device Driver with IOCTL, Synchronization, and State Management");
MODULE_VERSION("1.0");

#define DEVICE_NAME "smart_device"
#define CLASS_NAME  "smart_class"
#define BUFFER_SIZE 1024

/* Driver State and Data Structure */
struct smart_device_data {
    dev_t dev_num;                /* Major and Minor number */
    struct cdev cdev;             /* Character device structure */
    struct class *dev_class;      /* Device class for sysfs */
    struct device *dev;           /* Device structure for sysfs */
    char *buffer;                 /* Internal kernel buffer */
    size_t data_size;             /* Current amount of data in buffer */
    
    struct mutex lock;            /* Mutex for race condition protection */
    
    /* Configurable parameters & state */
    int mode;                     /* Operational mode */
    bool logging_enabled;         /* Toggle verbose logging */
    int timeout_ms;               /* Simulated timeout */
    char last_command[256];       /* History of last write */
    
    /* Statistics */
    struct smart_stats stats;     
};

static struct smart_device_data *smart_dev;

/* 
 * Helper macro for conditional logging 
 * Used to avoid spamming dmesg unless explicitly enabled via IOCTL
 */
#define SMART_LOG(fmt, ...) \
    do { \
        if (smart_dev && smart_dev->logging_enabled) \
            pr_info("smart_device: " fmt, ##__VA_ARGS__); \
    } while (0)

/* ========================================================================= */
/* File Operations Implementation                                            */
/* ========================================================================= */

static int smart_open(struct inode *inode, struct file *file)
{
    /* Associate the device context with the file pointer's private data */
    struct smart_device_data *dev_data = container_of(inode->i_cdev, struct smart_device_data, cdev);
    file->private_data = dev_data;

    mutex_lock(&dev_data->lock);
    dev_data->stats.open_count++;
    mutex_unlock(&dev_data->lock);

    SMART_LOG("Device opened. Open count: %lu\n", dev_data->stats.open_count);
    return 0;
}

static int smart_release(struct inode *inode, struct file *file)
{
    SMART_LOG("Device closed.\n");
    return 0; /* Successful close */
}

static ssize_t smart_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    struct smart_device_data *dev_data = file->private_data;
    size_t to_copy, not_copied;

    mutex_lock(&dev_data->lock); /* Protect shared buffer */

    /* Check if all data has been read */
    if (*offset >= dev_data->data_size) {
        mutex_unlock(&dev_data->lock);
        return 0; /* EOF */
    }

    /* Calculate how much to copy */
    to_copy = min(count, (size_t)(dev_data->data_size - *offset));

    /* Boundary check user space pointer and copy data */
    not_copied = copy_to_user(user_buffer, dev_data->buffer + *offset, to_copy);
    if (not_copied) {
        mutex_unlock(&dev_data->lock);
        pr_err("smart_device: Failed to copy %zu bytes to user space\n", not_copied);
        return -EFAULT;
    }

    *offset += to_copy;
    dev_data->stats.bytes_read += to_copy;
    
    SMART_LOG("Read %zu bytes. Offset now at %lld\n", to_copy, *offset);

    mutex_unlock(&dev_data->lock);
    return to_copy;
}

static ssize_t smart_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
    struct smart_device_data *dev_data = file->private_data;
    size_t to_copy, not_copied;

    mutex_lock(&dev_data->lock); /* Protect shared buffer and state */

    if (dev_data->mode == MODE_STRICT && count > BUFFER_SIZE) {
        mutex_unlock(&dev_data->lock);
        pr_warn("smart_device: STRICT mode active. Write rejected as it exceeds max buffer size.\n");
        return -EINVAL; /* Invalid argument */
    }

    /* Determine copy amount: overwrite buffer from beginning on new write */
    to_copy = min(count, (size_t)BUFFER_SIZE - 1);

    /* Copy data from user space */
    not_copied = copy_from_user(dev_data->buffer, user_buffer, to_copy);
    if (not_copied) {
        mutex_unlock(&dev_data->lock);
        pr_err("smart_device: Failed to copy %zu bytes from user space\n", not_copied);
        return -EFAULT;
    }

    dev_data->buffer[to_copy] = '\0'; /* Null-terminate string */
    dev_data->data_size = to_copy;

    /* Update history and stats */
    strncpy(dev_data->last_command, dev_data->buffer, sizeof(dev_data->last_command) - 1);
    dev_data->last_command[sizeof(dev_data->last_command) - 1] = '\0';
    dev_data->stats.bytes_written += to_copy;

    *offset = 0; /* Reset offset for subsequent reads */

    SMART_LOG("Written %zu bytes. Content: %s\n", to_copy, dev_data->buffer);

    mutex_unlock(&dev_data->lock);
    return to_copy;
}

static long smart_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct smart_device_data *dev_data = file->private_data;
    int err = 0;
    int tmp;

    /* Magic number validation */
    if (_IOC_TYPE(cmd) != SMART_MAGIC) {
        pr_warn("smart_device: IOCTL Magic Number Mismatch\n");
        return -ENOTTY; /* Not a valid ioctl command */
    }

    mutex_lock(&dev_data->lock); /* Protect state modifications */
    dev_data->stats.ioctl_count++;

    switch (cmd) {
        case RESET_DEVICE:
            SMART_LOG("IOCTL: RESET_DEVICE\n");
            memset(dev_data->buffer, 0, BUFFER_SIZE);
            dev_data->data_size = 0;
            dev_data->mode = MODE_NORMAL;
            dev_data->timeout_ms = 1000;
            memset(dev_data->last_command, 0, sizeof(dev_data->last_command));
            /* Do not reset stats or logging state on normal reset */
            break;

        case GET_DRIVER_STATS:
            SMART_LOG("IOCTL: GET_DRIVER_STATS\n");
            if (copy_to_user((struct smart_stats __user *)arg, &dev_data->stats, sizeof(struct smart_stats))) {
                err = -EFAULT;
            }
            break;

        case CLEAR_BUFFER:
            SMART_LOG("IOCTL: CLEAR_BUFFER\n");
            memset(dev_data->buffer, 0, BUFFER_SIZE);
            dev_data->data_size = 0;
            break;

        case SET_DEVICE_MODE:
            if (get_user(tmp, (int __user *)arg)) {
                err = -EFAULT;
            } else {
                if (tmp == MODE_NORMAL || tmp == MODE_STRICT || tmp == MODE_LOOPBACK) {
                    dev_data->mode = tmp;
                    SMART_LOG("IOCTL: SET_DEVICE_MODE to %d\n", tmp);
                } else {
                    err = -EINVAL; /* Invalid mode value */
                }
            }
            break;

        case GET_DEVICE_MODE:
            SMART_LOG("IOCTL: GET_DEVICE_MODE\n");
            if (put_user(dev_data->mode, (int __user *)arg)) {
                err = -EFAULT;
            }
            break;

        case ENABLE_LOGGING:
            dev_data->logging_enabled = true;
            pr_info("smart_device: Logging ENABLED via IOCTL\n");
            break;

        case DISABLE_LOGGING:
            pr_info("smart_device: Logging DISABLED via IOCTL\n");
            dev_data->logging_enabled = false;
            break;

        case GET_LAST_COMMAND:
            SMART_LOG("IOCTL: GET_LAST_COMMAND\n");
            if (copy_to_user((char __user *)arg, dev_data->last_command, sizeof(dev_data->last_command))) {
                err = -EFAULT;
            }
            break;

        case SET_TIMEOUT:
            if (get_user(tmp, (int __user *)arg)) {
                err = -EFAULT;
            } else {
                if (tmp < 0) {
                    err = -EINVAL; /* Negative timeout invalid */
                } else {
                    dev_data->timeout_ms = tmp;
                    SMART_LOG("IOCTL: SET_TIMEOUT to %d ms\n", tmp);
                }
            }
            break;

        case GET_TIMEOUT:
            SMART_LOG("IOCTL: GET_TIMEOUT\n");
            if (put_user(dev_data->timeout_ms, (int __user *)arg)) {
                err = -EFAULT;
            }
            break;

        default:
            pr_warn("smart_device: Unknown IOCTL command 0x%X\n", cmd);
            err = -ENOTTY;
            break;
    }

    mutex_unlock(&dev_data->lock);
    return err;
}

/* Connect file operations */
static const struct file_operations smart_fops = {
    .owner          = THIS_MODULE,
    .open           = smart_open,
    .release        = smart_release,
    .read           = smart_read,
    .write          = smart_write,
    .unlocked_ioctl = smart_ioctl,
};

/* ========================================================================= */
/* Module Initialization and Cleanup                                         */
/* ========================================================================= */

static int __init smart_driver_init(void)
{
    int ret;

    pr_info("smart_device: Initializing driver...\n");

    /* 1. Allocate memory for device structure */
    smart_dev = kzalloc(sizeof(struct smart_device_data), GFP_KERNEL);
    if (!smart_dev) {
        pr_err("smart_device: Failed to allocate memory for device structure\n");
        return -ENOMEM;
    }

    /* Allocate memory for internal buffer */
    smart_dev->buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!smart_dev->buffer) {
        pr_err("smart_device: Failed to allocate memory for buffer\n");
        kfree(smart_dev);
        return -ENOMEM;
    }

    /* Initialize Mutex */
    mutex_init(&smart_dev->lock);

    /* Initialize Default State */
    smart_dev->mode = MODE_NORMAL;
    smart_dev->logging_enabled = true;
    smart_dev->timeout_ms = 1000;
    
    /* 2. Dynamically allocate major number */
    ret = alloc_chrdev_region(&smart_dev->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("smart_device: Failed to allocate major number\n");
        goto unreg_mem;
    }
    pr_info("smart_device: Allocated Major Number: %d\n", MAJOR(smart_dev->dev_num));

    /* 3. Initialize and add cdev */
    cdev_init(&smart_dev->cdev, &smart_fops);
    smart_dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&smart_dev->cdev, smart_dev->dev_num, 1);
    if (ret < 0) {
        pr_err("smart_device: Failed to add cdev\n");
        goto unreg_chrdev;
    }

    /* 4. Create class for sysfs / udev support (creates /dev node automatically if udev is running) */
    smart_dev->dev_class = class_create(CLASS_NAME);
    if (IS_ERR(smart_dev->dev_class)) {
        pr_err("smart_device: Failed to create device class\n");
        ret = PTR_ERR(smart_dev->dev_class);
        goto unreg_cdev;
    }

    /* 5. Create device node */
    smart_dev->dev = device_create(smart_dev->dev_class, NULL, smart_dev->dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(smart_dev->dev)) {
        pr_err("smart_device: Failed to create device node\n");
        ret = PTR_ERR(smart_dev->dev);
        goto unreg_class;
    }

    pr_info("smart_device: Initialization completed successfully. Node created at /dev/%s\n", DEVICE_NAME);
    return 0;

    /* Error handling and cleanup paths */
unreg_class:
    class_destroy(smart_dev->dev_class);
unreg_cdev:
    cdev_del(&smart_dev->cdev);
unreg_chrdev:
    unregister_chrdev_region(smart_dev->dev_num, 1);
unreg_mem:
    kfree(smart_dev->buffer);
    kfree(smart_dev);
    return ret;
}

static void __exit smart_driver_exit(void)
{
    pr_info("smart_device: Unloading driver...\n");

    /* Cleanup sequence is exact reverse of initialization */
    if (smart_dev) {
        device_destroy(smart_dev->dev_class, smart_dev->dev_num);
        class_destroy(smart_dev->dev_class);
        cdev_del(&smart_dev->cdev);
        unregister_chrdev_region(smart_dev->dev_num, 1);
        
        mutex_destroy(&smart_dev->lock);
        kfree(smart_dev->buffer);
        kfree(smart_dev);
    }

    pr_info("smart_device: Driver unloaded successfully.\n");
}

module_init(smart_driver_init);
module_exit(smart_driver_exit);
