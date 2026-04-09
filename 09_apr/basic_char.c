#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "basic_char"
#define BUF_SIZE 256
static int major_number;
static char kernel_buffer[BUF_SIZE];
static int buffer_size;

static int basic_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "basic_char: device opened\n");
    return 0;
}

static int basic_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "basic_char: device closed\n");
    return 0;
}

static ssize_t basic_read(struct file *file, char __user *user_buffer, size_t count,loff_t *offset){
    int bytes_to_copy;
    if(*offset >=buffer_size)
    return 0;
    bytes_to_copy = min(count,(size_t)(buffer_size - *offset));

    if(copy_to_user(user_buffer, kernel_buffer + *offset,bytes_to_copy))
    return -EFAULT;
    *offset +=bytes_to_copy;
    printk(KERN_INFO "basic_char: read %d bytes\n",bytes_to_copy);
    return bytes_to_copy;
}

static ssize_t basic_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset){
    int bytes_to_copy;
    bytes_to_copy=min(count,(size_t)BUF_SIZE);
    if(copy_from_user(kernel_buffer,user_buffer,bytes_to_copy))
    return -EFAULT;
    buffer_size= bytes_to_copy;
    printk(KERN_INFO "basic_char: wrote %d bytes\n",bytes_to_copy);
    return bytes_to_copy;
}

static struct file_operations basic_fops=
{
    .owner = THIS_MODULE,
    .open = basic_open,
    .read =basic_read,
    .write= basic_write,
    .release= basic_release,
};

static int __init basic_char_init(void){
    major_number =register_chrdev(0, DEVICE_NAME, &basic_fops);
    if(major_number<0){
        printk(KERN_ERR "basic_char: failed to register device\n");
        return major_number;
    }
    printk(KERN_INFO "basic char: loaded\n");
    printk(KERN_INFO "basic char: major number =%d\n",major_number);
    printk(KERN_INFO "Create device node with: \n");
    printk(KERN_INFO "mknod /dev/%s c %d 0\n",DEVICE_NAME, major_number);
    return 0;
}

static void __exit basic_char_exit(void){
    unregister_chrdev(major_number,DEVICE_NAME);
    printk(KERN_INFO "basic_char: unloaded\n");
}

module_init(basic_char_init);
module_exit(basic_char_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("basic character driver with file opeations");

