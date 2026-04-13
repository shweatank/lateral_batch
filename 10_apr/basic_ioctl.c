#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "basic_ioctl"
#define IOCTL_MAGIC 'B'
#define IOCTL_SET_VALUE _IOWR(IOCTL_MAGIC, 1, int)

static int major;
static int kernel_value=0;

static long basic_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int user_value;
    switch (cmd)
    {
    case IOCTL_SET_VALUE:
        if(copy_from_user(&user_value, (int __user *)arg, sizeof(int))){
            return -EFAULT;
        }
        pr_info("kernel: received %d from user \n", user_value);
        kernel_value = user_value + 10;
        if(copy_to_user((int __user *)arg, &kernel_value, sizeof(int))){
            return -EFAULT;
        }
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static struct file_operations fops ={
    .owner = THIS_MODULE,
    .unlocked_ioctl = basic_ioctl,
};

static int __init basic_init(void){
    major =register_chrdev(0,DEVICE_NAME,&fops);
    pr_info("basic_ioctl loaded, major =%d\n",major);
    return 0;
}

static void __exit basic_exit(void){
    unregister_chrdev(major, DEVICE_NAME);
    pr_info("basic_ioctl unloaded\n");
}

module_init(basic_init);
module_exit(basic_exit);

MODULE_LICENSE("GPL");