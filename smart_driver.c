/*
 * smart_driver.c
 * --------------
 *  A "smart" character device driver demonstrating an industry-style
 *  layout: dynamic chrdev allocation, cdev + class + device_create,
 *  a kmalloc-backed internal buffer, mutex-based synchronization,
 *  IOCTL with multiple commands, statistics tracking, command history,
 *  configurable parameters, structured logging and proper cleanup.
 *
 *  Target: Linux kernel 6.x  (x86_64 / arm64)
 *  License: Dual BSD/GPL (driver itself is GPL — required for many
 *           kernel symbols we rely on, e.g. cdev helpers).
 *
 *  WHY a character driver?
 *  ---------------------
 *  Character ("char") devices expose a byte-stream interface to user
 *  space through /dev/<name>. Compared to block devices, they require
 *  no I/O scheduler or page-cache plumbing — perfect for sensors,
 *  controllers, virtual instruments, or (here) a teaching example.
 *
 *  Architectural overview
 *  ----------------------
 *
 *     User space                      Kernel space
 *  +----------------+             +------------------------+
 *  |  user_app      |  syscalls   |  VFS layer             |
 *  | open/read/...  | <---------> | (sys_open, sys_ioctl,..|
 *  +----------------+             +-----------+------------+
 *                                             |
 *                                             v
 *                                 +------------------------+
 *                                 |  smart_fops            |
 *                                 |  .open / .read / .write|
 *                                 |  .release / .ioctl     |
 *                                 +-----------+------------+
 *                                             |
 *                                             v
 *                       +-----------------------------------------+
 *                       |  Internal state (protected by mutex):   |
 *                       |    char *kbuf  (kmalloc'd 4 KiB)        |
 *                       |    struct smart_stats stats             |
 *                       |    enum smart_device_mode mode          |
 *                       |    unsigned int timeout_ms              |
 *                       |    bool logging_enabled                 |
 *                       |    struct smart_cmd_info last_cmd       |
 *                       +-----------------------------------------+
 *
 *  Concurrency
 *  -----------
 *  All shared state lives in a single struct (smart_dev) and is
 *  protected by ONE mutex. A mutex (vs spinlock) is appropriate here
 *  because:
 *    1. We call copy_to_user / copy_from_user which may sleep
 *       (page faults), so we MUST be in process context with sleep
 *       allowed — spinlocks would BUG_ON in such a case.
 *    2. None of our paths run from interrupt context.
 *
 *  Coding style follows Documentation/process/coding-style.rst:
 *    - tabs for indentation
 *    - lines <= 80 columns (mostly)
 *    - error labels for cleanup
 *    - one variable declaration per line at the top of each function
 */

#include <linux/module.h>      /* MODULE_*, module_init/exit            */
#include <linux/kernel.h>      /* pr_info / pr_err / KERN_* macros      */
#include <linux/init.h>        /* __init / __exit annotations           */
#include <linux/fs.h>          /* file_operations, register_chrdev_*    */
#include <linux/cdev.h>        /* cdev_init / cdev_add                  */
#include <linux/device.h>      /* class_create / device_create          */
#include <linux/slab.h>        /* kmalloc / kfree / kzalloc             */
#include <linux/uaccess.h>     /* copy_to_user / copy_from_user         */
#include <linux/mutex.h>       /* DEFINE_MUTEX, mutex_lock/unlock       */
#include <linux/errno.h>       /* -E... error constants                 */
#include <linux/string.h>      /* memset / memcpy / strscpy             */
#include <linux/jiffies.h>     /* jiffies (timestamp source)            */
#include <linux/version.h>     /* LINUX_VERSION_CODE / KERNEL_VERSION   */

#include "smart_ioctl.h"

/* ---------------------------------------------------------------------
 *  MODULE METADATA
 * ---------------------------------------------------------------------
 */
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Smart Device Project");
MODULE_DESCRIPTION("Industry-style char driver with IOCTL, stats, logging");
MODULE_VERSION("1.0");

/* ---------------------------------------------------------------------
 *  PER-DEVICE STATE
 *
 *  Everything the driver needs to remember between syscalls lives
 *  here. We use a single global instance (smart_dev) because we expose
 *  a single device node. Multi-instance drivers would put this struct
 *  inside file->private_data.
 * ---------------------------------------------------------------------
 */
struct smart_dev_state {
	/* Device-node bookkeeping */
	dev_t           devt;          /* major:minor allocated by alloc_chrdev_region */
	struct cdev     cdev;          /* kernel's cdev object                         */
	struct class   *class;         /* /sys/class/<name>                            */
	struct device  *device;        /* /dev/<name>                                  */

	/* Data path */
	char           *kbuf;          /* kmalloc'd internal buffer */
	size_t          data_len;      /* bytes currently stored    */

	/* Configurable parameters (accessible via IOCTL) */
	enum smart_device_mode mode;
	unsigned int    timeout_ms;
	bool            logging_enabled;

	/* Statistics & history */
	struct smart_stats     stats;
	struct smart_cmd_info  last_cmd;

	/* Synchronization */
	struct mutex    lock;          /* protects every field above */
};

static struct smart_dev_state smart_dev;

/* Helper: log only if logging is enabled. WHY: we want to allow runtime
 * silencing of the driver — useful when running under tight stress
 * tests to avoid filling dmesg. We use rate-limited variants for
 * paths that could spam the log under load. */
#define SMART_LOG(fmt, ...)                                                 \
	do {                                                                \
		if (smart_dev.logging_enabled)                              \
			pr_info("smart_device: " fmt, ##__VA_ARGS__);       \
	} while (0)

/* Always-on errors — these are rare and important. */
#define SMART_ERR(fmt, ...) \
	pr_err("smart_device: " fmt, ##__VA_ARGS__)

/* ---------------------------------------------------------------------
 *  HELPERS
 * ---------------------------------------------------------------------
 */
static const char *mode_to_str(enum smart_device_mode m)
{
	switch (m) {
	case SMART_MODE_NORMAL: return "NORMAL";
	case SMART_MODE_DEBUG:  return "DEBUG";
	case SMART_MODE_PERF:   return "PERF";
	case SMART_MODE_SAFE:   return "SAFE";
	default:                return "UNKNOWN";
	}
}

/* Record the most recent IOCTL command. Caller must hold the lock. */
static void record_last_cmd(int cmd, const char *desc)
{
	smart_dev.last_cmd.last_cmd  = cmd;
	smart_dev.last_cmd.timestamp = (u32)jiffies;
	strscpy(smart_dev.last_cmd.description, desc,
		sizeof(smart_dev.last_cmd.description));
}

/* ---------------------------------------------------------------------
 *  FILE OPERATIONS
 * ---------------------------------------------------------------------
 */

/*
 * smart_open()
 *  Called by the VFS each time user space opens /dev/smart_device.
 *  We simply bump the open counter; the buffer is already allocated
 *  in module init, so opening cannot fail under normal conditions.
 */
static int smart_open(struct inode *inode, struct file *filp)
{
	mutex_lock(&smart_dev.lock);
	smart_dev.stats.open_count++;
	mutex_unlock(&smart_dev.lock);

	SMART_LOG("open() pid=%d\n", current->pid);
	return 0;
}

/*
 * smart_release()
 *  Called when the LAST file descriptor referring to this inode is
 *  closed. (Multiple dup'd fds count as one open in VFS terms.)
 */
static int smart_release(struct inode *inode, struct file *filp)
{
	mutex_lock(&smart_dev.lock);
	smart_dev.stats.close_count++;
	mutex_unlock(&smart_dev.lock);

	SMART_LOG("release() pid=%d\n", current->pid);
	return 0;
}

/*
 * smart_read()
 *  Copy up to `count` bytes from our internal buffer to the user
 *  buffer `ubuf`. We use *ppos to support arbitrary seek positions;
 *  EOF is signalled by returning 0 once *ppos >= data_len.
 */
static ssize_t smart_read(struct file *filp, char __user *ubuf,
			  size_t count, loff_t *ppos)
{
	ssize_t ret;
	size_t  avail;
	size_t  to_copy;

	if (mutex_lock_interruptible(&smart_dev.lock))
		return -ERESTARTSYS;   /* user hit Ctrl-C while waiting */

	smart_dev.stats.read_count++;

	if (*ppos >= smart_dev.data_len) {
		ret = 0;   /* EOF */
		goto out;
	}

	avail   = smart_dev.data_len - *ppos;
	to_copy = min(count, avail);

	if (copy_to_user(ubuf, smart_dev.kbuf + *ppos, to_copy)) {
		smart_dev.stats.error_count++;
		ret = -EFAULT;
		goto out;
	}

	*ppos += to_copy;
	smart_dev.stats.bytes_read += to_copy;
	ret = to_copy;

	SMART_LOG("read() copied %zu bytes (pos now %lld)\n",
		  to_copy, *ppos);
out:
	mutex_unlock(&smart_dev.lock);
	return ret;
}

/*
 * smart_write()
 *  Replace the buffer contents with the data from user space.
 *  Writes longer than SMART_BUFFER_SIZE are truncated — we report
 *  back the number of bytes accepted, per the standard write(2)
 *  contract (partial writes are allowed).
 */
static ssize_t smart_write(struct file *filp, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	ssize_t ret;
	size_t  to_copy;

	if (count == 0)
		return 0;

	if (mutex_lock_interruptible(&smart_dev.lock))
		return -ERESTARTSYS;

	smart_dev.stats.write_count++;

	/* Boundary check: clamp to buffer size. SAFE mode would log a
	 * warning for any truncation; NORMAL mode is silent about it. */
	to_copy = min_t(size_t, count, (size_t)SMART_BUFFER_SIZE);

	if (smart_dev.mode == SMART_MODE_SAFE && to_copy < count)
		SMART_ERR("write truncated %zu -> %zu (SAFE mode)\n",
			  count, to_copy);

	if (copy_from_user(smart_dev.kbuf, ubuf, to_copy)) {
		smart_dev.stats.error_count++;
		ret = -EFAULT;
		goto out;
	}

	smart_dev.data_len           = to_copy;
	smart_dev.stats.bytes_written += to_copy;
	*ppos                         = to_copy;
	ret                           = to_copy;

	SMART_LOG("write() accepted %zu bytes\n", to_copy);
out:
	mutex_unlock(&smart_dev.lock);
	return ret;
}

/*
 * smart_ioctl()
 *  The "command channel" of the driver. Each command is identified by
 *  a (TYPE, NR) pair encoded in `cmd`. We:
 *    1. Validate the magic number.
 *    2. Validate the NR range.
 *    3. Dispatch to the right handler.
 *    4. Record the command in `last_cmd`.
 *
 *  WHY unlocked_ioctl (not the old .ioctl)?
 *    Linux removed the BKL-locked .ioctl entry point years ago.
 *    unlocked_ioctl is called WITHOUT the big kernel lock — the
 *    driver is responsible for its own synchronization. The name
 *    "unlocked" is now historical.
 */
static long smart_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	long ret = 0;
	void __user *uarg = (void __user *)arg;

	/* 1. Verify the magic number matches THIS driver.
	 *    _IOC_TYPE() extracts the 8-bit type field. */
	if (_IOC_TYPE(cmd) != SMART_IOC_MAGIC) {
		SMART_ERR("ioctl: bad magic 0x%x\n", _IOC_TYPE(cmd));
		return -ENOTTY;
	}

	/* 2. Range-check the sequence number. */
	if (_IOC_NR(cmd) > SMART_IOCTL_MAX_NR) {
		SMART_ERR("ioctl: bad NR %u\n", _IOC_NR(cmd));
		return -ENOTTY;
	}

	if (mutex_lock_interruptible(&smart_dev.lock))
		return -ERESTARTSYS;

	smart_dev.stats.ioctl_count++;

	switch (cmd) {
	case SMART_IOCTL_RESET_DEVICE:
		memset(smart_dev.kbuf, 0, SMART_BUFFER_SIZE);
		smart_dev.data_len   = 0;
		smart_dev.mode       = SMART_MODE_NORMAL;
		smart_dev.timeout_ms = 1000;
		memset(&smart_dev.stats, 0, sizeof(smart_dev.stats));
		record_last_cmd(cmd, "RESET_DEVICE");
		SMART_LOG("ioctl: RESET_DEVICE\n");
		break;

	case SMART_IOCTL_GET_STATS: {
		/* Copy a SNAPSHOT to user space. We are inside the
		 * mutex, so the struct can be read consistently. */
		if (copy_to_user(uarg, &smart_dev.stats,
				 sizeof(smart_dev.stats))) {
			ret = -EFAULT;
			smart_dev.stats.error_count++;
			break;
		}
		record_last_cmd(cmd, "GET_STATS");
		SMART_LOG("ioctl: GET_STATS\n");
		break;
	}

	case SMART_IOCTL_CLEAR_BUFFER:
		memset(smart_dev.kbuf, 0, SMART_BUFFER_SIZE);
		smart_dev.data_len = 0;
		record_last_cmd(cmd, "CLEAR_BUFFER");
		SMART_LOG("ioctl: CLEAR_BUFFER\n");
		break;

	case SMART_IOCTL_SET_MODE: {
		__s32 new_mode;

		if (copy_from_user(&new_mode, uarg, sizeof(new_mode))) {
			ret = -EFAULT;
			smart_dev.stats.error_count++;
			break;
		}
		if (new_mode < 0 || new_mode >= SMART_MODE_MAX) {
			ret = -EINVAL;
			smart_dev.stats.error_count++;
			break;
		}
		smart_dev.mode = (enum smart_device_mode)new_mode;
		record_last_cmd(cmd, "SET_MODE");
		SMART_LOG("ioctl: SET_MODE -> %s\n",
			  mode_to_str(smart_dev.mode));
		break;
	}

	case SMART_IOCTL_GET_MODE: {
		__s32 m = (__s32)smart_dev.mode;

		if (copy_to_user(uarg, &m, sizeof(m))) {
			ret = -EFAULT;
			smart_dev.stats.error_count++;
			break;
		}
		record_last_cmd(cmd, "GET_MODE");
		SMART_LOG("ioctl: GET_MODE = %s\n",
			  mode_to_str(smart_dev.mode));
		break;
	}

	case SMART_IOCTL_ENABLE_LOGGING:
		smart_dev.logging_enabled = true;
		record_last_cmd(cmd, "ENABLE_LOGGING");
		pr_info("smart_device: logging ENABLED\n");
		break;

	case SMART_IOCTL_DISABLE_LOGGING:
		pr_info("smart_device: logging DISABLED\n");
		record_last_cmd(cmd, "DISABLE_LOGGING");
		smart_dev.logging_enabled = false;
		break;

	case SMART_IOCTL_GET_LAST_CMD: {
		if (copy_to_user(uarg, &smart_dev.last_cmd,
				 sizeof(smart_dev.last_cmd))) {
			ret = -EFAULT;
			smart_dev.stats.error_count++;
			break;
		}
		/* Note: we record AFTER copy so we don't return our own
		 * GET_LAST_CMD as the answer to GET_LAST_CMD. */
		record_last_cmd(cmd, "GET_LAST_CMD");
		break;
	}

	case SMART_IOCTL_SET_TIMEOUT: {
		__u32 t;

		if (copy_from_user(&t, uarg, sizeof(t))) {
			ret = -EFAULT;
			smart_dev.stats.error_count++;
			break;
		}
		if (t == 0 || t > 60000) {           /* 0..60 s sensible range */
			ret = -EINVAL;
			smart_dev.stats.error_count++;
			break;
		}
		smart_dev.timeout_ms = t;
		record_last_cmd(cmd, "SET_TIMEOUT");
		SMART_LOG("ioctl: SET_TIMEOUT = %u ms\n", t);
		break;
	}

	case SMART_IOCTL_GET_TIMEOUT: {
		__u32 t = smart_dev.timeout_ms;

		if (copy_to_user(uarg, &t, sizeof(t))) {
			ret = -EFAULT;
			smart_dev.stats.error_count++;
			break;
		}
		record_last_cmd(cmd, "GET_TIMEOUT");
		break;
	}

	default:
		/* Magic + NR passed our checks but the command itself
		 * is unknown. ENOTTY is the canonical "no such ioctl"
		 * errno; the historical name stands for "not a
		 * teletype" but POSIX redefines it for ioctl misuse. */
		smart_dev.stats.error_count++;
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&smart_dev.lock);
	return ret;
}

/* The single dispatch table the kernel uses to route syscalls. */
static const struct file_operations smart_fops = {
	.owner          = THIS_MODULE,
	.open           = smart_open,
	.release        = smart_release,
	.read           = smart_read,
	.write          = smart_write,
	.unlocked_ioctl = smart_ioctl,
	.compat_ioctl   = smart_ioctl,  /* 32-bit on 64-bit kernel */
	.llseek         = default_llseek,
};

/* ---------------------------------------------------------------------
 *  MODULE INIT / EXIT
 *
 *  These are the only functions called by the kernel directly via
 *  module_init / module_exit. They must:
 *    - acquire every resource in a checked, ordered way;
 *    - release them in REVERSE order on error;
 *    - never leak on the failure path.
 * ---------------------------------------------------------------------
 */
static int __init smart_init(void)
{
	int err;

	pr_info("smart_device: loading driver v1.0\n");

	mutex_init(&smart_dev.lock);
	smart_dev.mode            = SMART_MODE_NORMAL;
	smart_dev.timeout_ms      = 1000;
	smart_dev.logging_enabled = true;

	/* 1. Allocate the internal byte buffer. */
	smart_dev.kbuf = kzalloc(SMART_BUFFER_SIZE, GFP_KERNEL);
	if (!smart_dev.kbuf) {
		err = -ENOMEM;
		goto err_kbuf;
	}

	/* 2. Reserve a (major, minor) range dynamically. Dynamic
	 *    allocation is preferred over hard-coded majors because the
	 *    kernel does not guarantee any particular major is free. */
	err = alloc_chrdev_region(&smart_dev.devt, 0, 1, SMART_DEVICE_NAME);
	if (err < 0) {
		SMART_ERR("alloc_chrdev_region failed: %d\n", err);
		goto err_region;
	}
	pr_info("smart_device: registered as major=%d minor=%d\n",
		MAJOR(smart_dev.devt), MINOR(smart_dev.devt));

	/* 3. Bind the file_operations to the cdev. */
	cdev_init(&smart_dev.cdev, &smart_fops);
	smart_dev.cdev.owner = THIS_MODULE;

	err = cdev_add(&smart_dev.cdev, smart_dev.devt, 1);
	if (err < 0) {
		SMART_ERR("cdev_add failed: %d\n", err);
		goto err_cdev;
	}

	/* 4. Create /sys/class/<name>. Note: signature changed in
	 *    kernel 6.4 — class_create() lost its first THIS_MODULE
	 *    argument. We guard with LINUX_VERSION_CODE for portability. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	smart_dev.class = class_create(SMART_CLASS_NAME);
#else
	smart_dev.class = class_create(THIS_MODULE, SMART_CLASS_NAME);
#endif
	if (IS_ERR(smart_dev.class)) {
		err = PTR_ERR(smart_dev.class);
		SMART_ERR("class_create failed: %d\n", err);
		goto err_class;
	}

	/* 5. Ask udev (via /sys/class) to create /dev/<name>. */
	smart_dev.device = device_create(smart_dev.class, NULL,
					 smart_dev.devt, NULL,
					 SMART_DEVICE_NAME);
	if (IS_ERR(smart_dev.device)) {
		err = PTR_ERR(smart_dev.device);
		SMART_ERR("device_create failed: %d\n", err);
		goto err_device;
	}

	pr_info("smart_device: /dev/%s ready\n", SMART_DEVICE_NAME);
	return 0;

err_device:
	class_destroy(smart_dev.class);
err_class:
	cdev_del(&smart_dev.cdev);
err_cdev:
	unregister_chrdev_region(smart_dev.devt, 1);
err_region:
	kfree(smart_dev.kbuf);
err_kbuf:
	mutex_destroy(&smart_dev.lock);
	return err;
}

static void __exit smart_exit(void)
{
	/* Tear down in the REVERSE order of init. */
	device_destroy(smart_dev.class, smart_dev.devt);
	class_destroy(smart_dev.class);
	cdev_del(&smart_dev.cdev);
	unregister_chrdev_region(smart_dev.devt, 1);
	kfree(smart_dev.kbuf);
	mutex_destroy(&smart_dev.lock);

	pr_info("smart_device: unloaded\n");
}

module_init(smart_init);
module_exit(smart_exit);
