#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>



static int major_number;

static struct file_operations fops = {
};

static int __init sample_init(void) {
    major_number = register_chrdev(0, "sampledriver", &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Failed to register char device\n");
        return major_number;
    }

    printk(KERN_INFO "basic char driver loaded \n");
    return 0;
}

static void __exit sample_exit(void) {
    unregister_chrdev(major_number, "sampledriver");
    printk(KERN_INFO "Exiting\n");
}

module_init(sample_init);
module_exit(sample_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("A sample character driver");
