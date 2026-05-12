#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include "calc_ioctl.h"

#define DEVICE_NAME "calc_dev"
#define CLASS_NAME  "calc_class"

static dev_t dev_num;
static struct cdev calc_cdev;
static struct class *calc_class;

static int calc_open(struct inode *inode, struct file *file)
{
	pr_info("calc_driver: device opened\n");
	return 0;
}

static int calc_release(struct inode *inode, struct file *file)
{
	pr_info("calc_driver: device closed\n");
	return 0;
}

static long calc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct calc_data data;

	switch (cmd) {
	case CALC_COMPUTE:
		if (copy_from_user(&data, (struct calc_data __user *)arg,
				   sizeof(data)))
			return -EFAULT;

		switch (data.op) {
		case CALC_OP_ADD:
			data.result = data.a + data.b;
			break;
		case CALC_OP_SUB:
			data.result = data.a - data.b;
			break;
		default:
			pr_warn("calc_driver: invalid op %d\n", data.op);
			return -EINVAL;
		}

		pr_info("calc_driver: %d %s %d = %d\n",
			data.a, data.op == CALC_OP_ADD ? "+" : "-",
			data.b, data.result);

		if (copy_to_user((struct calc_data __user *)arg, &data,
				 sizeof(data)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static const struct file_operations calc_fops = {
	.owner          = THIS_MODULE,
	.open           = calc_open,
	.release        = calc_release,
	.unlocked_ioctl = calc_ioctl,
};

static int __init calc_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		pr_err("calc_driver: failed to alloc chrdev\n");
		return ret;
	}

	cdev_init(&calc_cdev, &calc_fops);
	calc_cdev.owner = THIS_MODULE;

	ret = cdev_add(&calc_cdev, dev_num, 1);
	if (ret < 0) {
		unregister_chrdev_region(dev_num, 1);
		return ret;
	}

	calc_class = class_create(CLASS_NAME);
	if (IS_ERR(calc_class)) {
		cdev_del(&calc_cdev);
		unregister_chrdev_region(dev_num, 1);
		return PTR_ERR(calc_class);
	}

	device_create(calc_class, NULL, dev_num, NULL, DEVICE_NAME);

	pr_info("calc_driver: loaded, major=%d minor=%d\n",
		MAJOR(dev_num), MINOR(dev_num));
	return 0;
}

static void __exit calc_exit(void)
{
	device_destroy(calc_class, dev_num);
	class_destroy(calc_class);
	cdev_del(&calc_cdev);
	unregister_chrdev_region(dev_num, 1);
	pr_info("calc_driver: unloaded\n");
}

module_init(calc_init);
module_exit(calc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("demo");
MODULE_DESCRIPTION("Simple add/sub calculator char driver");
