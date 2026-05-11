/*
 * smart_driver.c - Industry-grade character device driver for /dev/smart_device
 *
 * Features:
 *   - Dynamic char major number allocation
 *   - cdev + sysfs class so udev creates /dev/smart_device automatically
 *   - 4 KiB internal kernel buffer (kmalloc'd at module load)
 *   - read()/write() with copy_to_user / copy_from_user
 *   - 10 distinct ioctl commands using _IO/_IOR/_IOW direction macros
 *   - Per-device statistics (opens, reads, writes, ioctls, bytes, errors)
 *   - 16-entry ring of recent ioctl command history
 *   - Configurable mode and timeout
 *   - mutex-based mutual exclusion guards every shared field
 *   - Full cleanup path on every error in the init flow
 *   - LINUX_VERSION_CODE guard so it compiles on both 6.4+ (single-arg
 *     class_create) and older 6.x kernels (two-arg form)
 *
 * Build: see Makefile in this directory.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/errno.h>

#include "smart_ioctl.h"

/* ---------------------------------------------------------------- */
/* Compile-time constants                                            */
/* ---------------------------------------------------------------- */
#define DRIVER_NAME             "smart_device"
#define DRIVER_CLASS            "smart_class"
#define SMART_BUF_SIZE          4096    /* internal kernel buffer    */
#define SMART_HIST_SIZE         16      /* ring of recent ioctls     */
#define SMART_DEFAULT_TIMEOUT   1000    /* milliseconds              */
#define SMART_MAX_TIMEOUT       60000   /* 60 s upper bound          */

/* ---------------------------------------------------------------- */
/* Per-device state                                                  */
/*                                                                   */
/* Everything that may be touched from more than one context lives   */
/* behind smart_dev::lock. Reads and writes to scalar counters are   */
/* covered by the same mutex so the snapshot returned by             */
/* SMART_GET_DRIVER_STATS is internally consistent.                  */
/* ---------------------------------------------------------------- */
struct smart_dev {
	/* I/O buffer */
	char                   *buffer;
	size_t                  data_len;     /* valid bytes in buffer */

	/* Configurable parameters */
	enum smart_mode         mode;
	bool                    logging;
	u32                     timeout_ms;

	/* Statistics */
	struct smart_stats      stats;

	/* Recent command history (ring) */
	struct smart_cmd_entry  history[SMART_HIST_SIZE];
	u32                     history_head;  /* next slot to write   */
	u32                     history_count; /* saturates at SMART_HIST_SIZE */

	/* Synchronisation */
	struct mutex            lock;

	/* Char device plumbing */
	dev_t                   devt;
	struct cdev             cdev;
	struct class           *class;
	struct device          *dev;
};

/*
 * Single global pointer is fine here because we register exactly one
 * device. Multi-instance drivers would key off inode->i_cdev instead.
 */
static struct smart_dev *gdev;

/* ---------------------------------------------------------------- */
/* Logging helper                                                    */
/* ---------------------------------------------------------------- */
/*
 * smart_log() emits a kernel message only when:
 *   1. logging is currently enabled AND
 *   2. the device is not in SILENT mode.
 * Using pr_info_ratelimited keeps an angry test from flooding dmesg.
 */
#define smart_log(d, fmt, ...) do {                                  \
	if ((d)->logging && (d)->mode != SMART_MODE_SILENT)          \
		pr_info_ratelimited(DRIVER_NAME ": " fmt,            \
				    ##__VA_ARGS__);                  \
} while (0)

/* ---------------------------------------------------------------- */
/* History helper - caller must hold d->lock                         */
/* ---------------------------------------------------------------- */
static void smart_history_add(struct smart_dev *d, u32 cmd_nr, int result)
{
	struct smart_cmd_entry *e = &d->history[d->history_head];

	e->cmd_nr       = cmd_nr;
	e->result       = result;
	e->timestamp_ns = ktime_get_real_ns();

	d->history_head = (d->history_head + 1) % SMART_HIST_SIZE;
	if (d->history_count < SMART_HIST_SIZE)
		d->history_count++;
}

/* ---------------------------------------------------------------- */
/* file_operations callbacks                                         */
/* ---------------------------------------------------------------- */

static int smart_open(struct inode *inode, struct file *file)
{
	struct smart_dev *d = container_of(inode->i_cdev,
					   struct smart_dev, cdev);

	file->private_data = d;

	mutex_lock(&d->lock);
	d->stats.opens++;
	mutex_unlock(&d->lock);

	smart_log(d, "open() pid=%d\n", current->pid);
	return 0;
}

static int smart_release(struct inode *inode, struct file *file)
{
	struct smart_dev *d = file->private_data;

	mutex_lock(&d->lock);
	d->stats.closes++;
	mutex_unlock(&d->lock);

	smart_log(d, "release() pid=%d\n", current->pid);
	return 0;
}

static ssize_t smart_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct smart_dev *d = file->private_data;
	ssize_t ret;
	size_t  to_copy;

	if (mutex_lock_interruptible(&d->lock))
		return -ERESTARTSYS;

	/* EOF when the file position has reached the valid data length */
	if (*ppos >= (loff_t)d->data_len) {
		ret = 0;
		goto out;
	}

	to_copy = min(count, d->data_len - (size_t)*ppos);

	if (copy_to_user(buf, d->buffer + *ppos, to_copy)) {
		d->stats.errors++;
		ret = -EFAULT;
		goto out;
	}

	*ppos               += to_copy;
	d->stats.reads      += 1;
	d->stats.bytes_read += to_copy;
	ret                  = to_copy;

out:
	mutex_unlock(&d->lock);
	smart_log(d, "read() -> %zd\n", ret);
	return ret;
}

static ssize_t smart_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct smart_dev *d = file->private_data;
	ssize_t ret;
	size_t  to_copy;

	if (count == 0)
		return 0;

	if (mutex_lock_interruptible(&d->lock))
		return -ERESTARTSYS;

	/* Bound the write to the buffer size - silently truncate */
	to_copy = min(count, (size_t)SMART_BUF_SIZE);

	if (copy_from_user(d->buffer, buf, to_copy)) {
		d->stats.errors++;
		ret = -EFAULT;
		goto out;
	}

	d->data_len             = to_copy;
	*ppos                   = to_copy;
	d->stats.writes        += 1;
	d->stats.bytes_written += to_copy;
	ret                     = to_copy;

out:
	mutex_unlock(&d->lock);
	smart_log(d, "write() -> %zd\n", ret);
	return ret;
}

/* ---------------------------------------------------------------- */
/* IOCTL dispatcher                                                  */
/* ---------------------------------------------------------------- */

static long smart_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct smart_dev *d = file->private_data;
	void __user *uarg = (void __user *)arg;
	long ret = 0;
	u32  v32;

	/* 1. Validate that this ioctl is one of ours. */
	if (_IOC_TYPE(cmd) != SMART_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) == 0 || _IOC_NR(cmd) > SMART_IOC_MAXNR)
		return -ENOTTY;

	/* 2. Verify the user pointer is accessible. access_ok() only
	 *    checks the address range; copy_*_user does the actual
	 *    fault handling. Skipping for _IO (no payload) commands. */
	if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE)) {
		if (!access_ok(uarg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	if (mutex_lock_interruptible(&d->lock))
		return -ERESTARTSYS;

	d->stats.ioctls++;

	switch (cmd) {

	case SMART_RESET_DEVICE:
		/* Wipe buffer, restore defaults, clear stats EXCEPT this call */
		memset(d->buffer, 0, SMART_BUF_SIZE);
		d->data_len      = 0;
		d->mode          = SMART_MODE_NORMAL;
		d->timeout_ms    = SMART_DEFAULT_TIMEOUT;
		d->history_head  = 0;
		d->history_count = 0;
		memset(&d->stats, 0, sizeof(d->stats));
		d->stats.ioctls  = 1;
		break;

	case SMART_GET_DRIVER_STATS:
		if (copy_to_user(uarg, &d->stats, sizeof(d->stats)))
			ret = -EFAULT;
		break;

	case SMART_CLEAR_BUFFER:
		memset(d->buffer, 0, SMART_BUF_SIZE);
		d->data_len = 0;
		break;

	case SMART_SET_DEVICE_MODE:
		if (copy_from_user(&v32, uarg, sizeof(v32))) {
			ret = -EFAULT;
			break;
		}
		if (v32 >= SMART_MODE_MAX) {
			ret = -EINVAL;
			break;
		}
		d->mode = v32;
		break;

	case SMART_GET_DEVICE_MODE:
		v32 = (u32)d->mode;
		if (copy_to_user(uarg, &v32, sizeof(v32)))
			ret = -EFAULT;
		break;

	case SMART_ENABLE_LOGGING:
		d->logging = true;
		break;

	case SMART_DISABLE_LOGGING:
		d->logging = false;
		break;

	case SMART_GET_LAST_COMMAND: {
		u32 last;

		if (d->history_count == 0) {
			ret = -ENODATA;
			break;
		}
		last = (d->history_head + SMART_HIST_SIZE - 1) %
		       SMART_HIST_SIZE;
		if (copy_to_user(uarg, &d->history[last],
				 sizeof(struct smart_cmd_entry)))
			ret = -EFAULT;
		break;
	}

	case SMART_SET_TIMEOUT:
		if (copy_from_user(&v32, uarg, sizeof(v32))) {
			ret = -EFAULT;
			break;
		}
		if (v32 == 0 || v32 > SMART_MAX_TIMEOUT) {
			ret = -EINVAL;
			break;
		}
		d->timeout_ms = v32;
		break;

	case SMART_GET_TIMEOUT:
		v32 = d->timeout_ms;
		if (copy_to_user(uarg, &v32, sizeof(v32)))
			ret = -EFAULT;
		break;

	default:
		/* Should be unreachable thanks to the _IOC_NR check above */
		ret = -ENOTTY;
	}

	/* Record this command in the history ring */
	smart_history_add(d, _IOC_NR(cmd), (int)ret);
	if (ret < 0)
		d->stats.errors++;

	mutex_unlock(&d->lock);

	smart_log(d, "ioctl nr=%u ret=%ld\n", _IOC_NR(cmd), ret);
	return ret;
}

static const struct file_operations smart_fops = {
	.owner          = THIS_MODULE,
	.open           = smart_open,
	.release        = smart_release,
	.read           = smart_read,
	.write          = smart_write,
	.unlocked_ioctl = smart_ioctl,
	.llseek         = default_llseek,
};

/* ---------------------------------------------------------------- */
/* Module init / exit                                                */
/* ---------------------------------------------------------------- */

static int __init smart_init(void)
{
	int ret;

	/* Step 1: per-device state */
	gdev = kzalloc(sizeof(*gdev), GFP_KERNEL);
	if (!gdev)
		return -ENOMEM;

	gdev->buffer = kzalloc(SMART_BUF_SIZE, GFP_KERNEL);
	if (!gdev->buffer) {
		ret = -ENOMEM;
		goto err_free_dev;
	}

	gdev->mode       = SMART_MODE_NORMAL;
	gdev->logging    = true;
	gdev->timeout_ms = SMART_DEFAULT_TIMEOUT;
	mutex_init(&gdev->lock);

	/* Step 2: reserve a major/minor */
	ret = alloc_chrdev_region(&gdev->devt, 0, 1, DRIVER_NAME);
	if (ret < 0) {
		pr_err(DRIVER_NAME ": alloc_chrdev_region failed: %d\n", ret);
		goto err_free_buf;
	}

	/* Step 3: register cdev */
	cdev_init(&gdev->cdev, &smart_fops);
	gdev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&gdev->cdev, gdev->devt, 1);
	if (ret < 0) {
		pr_err(DRIVER_NAME ": cdev_add failed: %d\n", ret);
		goto err_unreg_chrdev;
	}

	/* Step 4: create class so udev creates /dev/smart_device. The
	 * class_create() prototype changed in v6.4: it dropped the
	 * leading struct module * argument. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	gdev->class = class_create(DRIVER_CLASS);
#else
	gdev->class = class_create(THIS_MODULE, DRIVER_CLASS);
#endif
	if (IS_ERR(gdev->class)) {
		ret = PTR_ERR(gdev->class);
		pr_err(DRIVER_NAME ": class_create failed: %d\n", ret);
		goto err_del_cdev;
	}

	/* Step 5: create device node */
	gdev->dev = device_create(gdev->class, NULL, gdev->devt, NULL,
				  DRIVER_NAME);
	if (IS_ERR(gdev->dev)) {
		ret = PTR_ERR(gdev->dev);
		pr_err(DRIVER_NAME ": device_create failed: %d\n", ret);
		goto err_destroy_class;
	}

	pr_info(DRIVER_NAME ": loaded (major=%d minor=%d, buf=%d bytes)\n",
		MAJOR(gdev->devt), MINOR(gdev->devt), SMART_BUF_SIZE);
	return 0;

err_destroy_class:
	class_destroy(gdev->class);
err_del_cdev:
	cdev_del(&gdev->cdev);
err_unreg_chrdev:
	unregister_chrdev_region(gdev->devt, 1);
err_free_buf:
	kfree(gdev->buffer);
err_free_dev:
	mutex_destroy(&gdev->lock);
	kfree(gdev);
	gdev = NULL;
	return ret;
}

static void __exit smart_exit(void)
{
	device_destroy(gdev->class, gdev->devt);
	class_destroy(gdev->class);
	cdev_del(&gdev->cdev);
	unregister_chrdev_region(gdev->devt, 1);
	kfree(gdev->buffer);
	mutex_destroy(&gdev->lock);
	kfree(gdev);
	gdev = NULL;
	pr_info(DRIVER_NAME ": unloaded\n");
}

module_init(smart_init);
module_exit(smart_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("smart_device project");
MODULE_DESCRIPTION("Industry-grade Linux char driver with full IOCTL suite");
MODULE_VERSION("1.0");
