#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "reverse_char"
#define BUF_SIZE 256
static int major_number;
static char kernel_buffer[BUF_SIZE];
static int buffer_size;

static int reverse_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "reverse_char: device opened\n");
    return 0;
}

static int reverse_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "reverse_char: device closed\n");
    return 0;
}

static ssize_t reverse_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset){
    int bytes_to_copy;
    if(*offset >= buffer_size)
        return 0;
    bytes_to_copy = min(count, (size_t)(buffer_size - *offset));

    if(copy_to_user(user_buffer, kernel_buffer + *offset, bytes_to_copy))
        return -EFAULT;
    
    *offset += bytes_to_copy;
    printk(KERN_INFO "reverse_char: read %d bytes\n", bytes_to_copy);
    return bytes_to_copy;
}

static ssize_t reverse_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset){
    int bytes_to_copy;
    int len, i, j;
    char temp;

    bytes_to_copy = min(count, (size_t)(BUF_SIZE - 1));
    if(copy_from_user(kernel_buffer, user_buffer, bytes_to_copy))
        return -EFAULT;
    
    kernel_buffer[bytes_to_copy] = '\0';
    
    len = bytes_to_copy;
    // Don't reverse the newline character if it's there
    if (len > 0 && kernel_buffer[len - 1] == '\n') {
        len--;
    }

    // Reverse the string
    for (i = 0, j = len - 1; i < j; i++, j--) {
        temp = kernel_buffer[i];
        kernel_buffer[i] = kernel_buffer[j];
        kernel_buffer[j] = temp;
    }

    buffer_size = bytes_to_copy;
    printk(KERN_INFO "reverse_char: Received string, reversed it to: %s\n", kernel_buffer);
    
    return bytes_to_copy;
}

static struct file_operations reverse_fops=
{
    .owner = THIS_MODULE,
    .open = reverse_open,
    .read = reverse_read,
    .write= reverse_write,
    .release= reverse_release,
};

static int __init reverse_char_init(void){
    major_number = register_chrdev(0, DEVICE_NAME, &reverse_fops);
    if(major_number < 0){
        printk(KERN_ERR "reverse_char: failed to register device\n");
        return major_number;
    }
    printk(KERN_INFO "reverse_char: loaded\n");
    printk(KERN_INFO "reverse_char: major number = %d\n", major_number);
    printk(KERN_INFO "Create device node with: \n");
    printk(KERN_INFO "mknod /dev/%s c %d 0\n", DEVICE_NAME, major_number);
    return 0;
}

static void __exit reverse_char_exit(void){
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "reverse_char: unloaded\n");
}

module_init(reverse_char_init);
module_exit(reverse_char_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("character driver that reverses strings");
