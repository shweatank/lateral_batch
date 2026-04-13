#include<linux/module.h>
#include<linux/init.h>
#include<linux/kernel.h>
#include<linux/fs.h>     //register_chrdev,fileoperaations
#include<linux/uaccess.h>   //copy to iuser ,copy from user




#define DEVICE_NAME "ioctl_cal"
#define IOCTL_MAGIC 'C'
#define IOCTL_SET_VALUE _IOWR(IOCTL_MAGIC,1,struct calc_data)

struct calc_data {
    int a;
    int b;
    char op;     
    int result;
};

static int major;
static int kernel_value=0;

static long calc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct calc_data data;

    

    if (copy_from_user(&data, (struct calc_data __user *)arg, sizeof(data)))
        return -EFAULT;

    printk(KERN_INFO "Kernel received: a=%d, b=%d, op=%c\n", data.a, data.b, data.op);

    switch (data.op) {
        case '+':
            data.result = data.a + data.b;
            break;
        case '-':
            data.result = data.a - data.b;
            break;
        case '*':
            data.result = data.a * data.b;
            break;
        case '/':
            if (data.b == 0)
                return -EINVAL;
            data.result = data.a / data.b;
            break;
        default:
            return -EINVAL;
    }

    if (copy_to_user((struct calc_data __user *)arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}


 static struct file_operations basic_fops = {
    .owner= THIS_MODULE,
    
    
    
    .unlocked_ioctl = calc_ioctl,
};
static int __init basic_init(void)
{
major=register_chrdev(0,DEVICE_NAME,&basic_fops);
printk(KERN_INFO "Basic ioctl loaded major number =%d\n",major);
return 0;
}

static void __exit basic_exit(void)	
{
unregister_chrdev(major,DEVICE_NAME);
printk(KERN_INFO"basic_ioctl:unloaded\n");
}

module_init(basic_init);
module_exit(basic_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("ioctl");


