#include <linux/module.h>        // Core header for loading LKMs
#include <linux/kernel.h>        // printk()
#include <linux/init.h>          // __init, __exit
#include <linux/uaccess.h>       // copy_from_user()
#include <linux/fs.h>            // file operations
#include <linux/cdev.h>          // char device
#include <linux/device.h>        // device creation
#include <linux/interrupt.h>     // IRQ handling
#include <linux/workqueue.h>     // workqueue
#include <linux/kthread.h>       // kernel thread
#include <linux/delay.h>         // msleep()

#define DEVICE_NAME "irq_demo"   // Device name

static int major;                // Major number
static struct class *cls;        // Device class

static int user_val1, user_val2; // Input values
static int result;               // Final result
static int op_flag = 0;          // Operation flag

static struct work_struct my_work;   // Workqueue structure
static struct task_struct *thread;  // Kernel thread

// ---------------- IRQ HANDLER ----------------
irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    printk("IRQ: Interrupt triggered\n"); // Log message

    // Schedule bottom half work
    schedule_work(&my_work);

    return IRQ_HANDLED; // Interrupt handled
}

// ---------------- WORKQUEUE -----------------
void work_handler(struct work_struct *work)
{
    printk("WORKQUEUE: Processing operation\n");

    switch (op_flag) // Decide operation
    {
        case 1:
            result = user_val1 + user_val2; // Addition
            printk("ADD operation\n");
            break;

        case 2:
            result = user_val1 - user_val2; // Subtraction
            printk("SUB operation\n");
            break;

        case 3:
            result = user_val1 * user_val2; // Multiplication
            printk("MUL operation\n");
            break;

        case 4:
            if (user_val2 != 0)
            {
                result = user_val1 / user_val2; // Division
                printk("DIV operation\n");
            }
            else
            {
                printk("Error: Divide by zero\n");
                result = 0;
            }
            break;

        default:
            printk("Invalid operation\n");
            result = 0;
    }
}

// ---------------- THREAD --------------------
int thread_fn(void *data)
{
    while (!kthread_should_stop()) // Loop until stopped
    {
        printk("THREAD: Result = %d\n", result); // Print result

        msleep(2000); // Sleep 2 seconds
    }
    return 0;
}

// ---------------- WRITE (USER → KERNEL) -----
static ssize_t dev_write(struct file *file,
                        const char __user *buf,
                        size_t len, loff_t *off)
{
    int data[3]; // val1, val2, op_flag
	printk("WRITE FUNCATION CALLED\n");

    // Copy data from user space
    if (copy_from_user(data, buf, sizeof(data)))
        return -EFAULT;

    user_val1 = data[0]; // First value
    user_val2 = data[1]; // Second value
    op_flag   = data[2]; // Operation type

    printk("USER: val1=%d val2=%d op=%d\n",
            user_val1, user_val2, op_flag);

    // Trigger simulated IRQ
    my_irq_handler(0, NULL);

    return len;
}

// ---------------- FILE OPERATIONS -----------
static struct file_operations fops =
{
    .owner = THIS_MODULE,
    .write = dev_write,
};

// ---------------- INIT ----------------------
static int __init my_init(void)
{
    printk("Module Loaded\n");

    // Register character device
    major = register_chrdev(0, DEVICE_NAME, &fops);

    // Create class
    cls = class_create("irq_class");

    // Create device file: /dev/irq_demo
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    // Initialize workqueue
    INIT_WORK(&my_work, work_handler);

    // Create kernel thread
    thread = kthread_run(thread_fn, NULL, "irq_thread");

    return 0;
}

// ---------------- EXIT ----------------------
static void __exit my_exit(void)
{
    kthread_stop(thread); // Stop thread

    device_destroy(cls, MKDEV(major, 0)); // Remove device
    class_destroy(cls);                   // Destroy class
    unregister_chrdev(major, DEVICE_NAME);// Unregister device

    printk("Module Unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
