#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/string.h>

#define DEVICE_NAME "task_driver"
#define BUF_SIZE 256
#define KBD_IRQ 1
#define KBD_DATA_PORT 0x60

static int major_number;
static char kernel_buffer[BUF_SIZE];
static int string_received_flag = 0;
static int tasklet_done_flag = 0;

static struct task_struct *print_thread_ts;
static DECLARE_WAIT_QUEUE_HEAD(wq);

static void my_tasklet_func(struct tasklet_struct *t) {
    int len = strlen(kernel_buffer);
    int i, j;
    char temp;

    for (i = 0, j = len - 1; i < j; i++, j--) {
        temp = kernel_buffer[i];
        kernel_buffer[i] = kernel_buffer[j];
        kernel_buffer[j] = temp;
    }

    tasklet_done_flag = 1;
    wake_up_interruptible(&wq);
}

DECLARE_TASKLET(my_tasklet, my_tasklet_func);

// Keyboard IRQ handler
static irqreturn_t kbd_irq_handler(int irq, void *dev_id) {
    unsigned char scancode;
    
    scancode = inb(KBD_DATA_PORT);

    if (scancode & 0x80) {
        return IRQ_HANDLED;
    }

    if (string_received_flag) {
        string_received_flag = 0; 
        pr_info("task_driver: Keyboard pressed, scheduling tasklet...\n");
        tasklet_schedule(&my_tasklet);
    }

    return IRQ_HANDLED;
}

static int print_thread_func(void *data) {
    pr_info("task_driver: Print thread started\n");
    
    while (!kthread_should_stop()) {
        wait_event_interruptible(wq, tasklet_done_flag || kthread_should_stop());
        
        if (tasklet_done_flag) {
            pr_info("task_driver: Tasklet finished. Reversed String -> %s\n", kernel_buffer);
            tasklet_done_flag = 0; 
        }
    }
    
    pr_info("task_driver: Print thread stopping\n");
    return 0;
}

// File operations
static int dev_open(struct inode *inode, struct file *file) {
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset) {
    int bytes_to_copy;

    bytes_to_copy = min(count, (size_t)(BUF_SIZE - 1));
    if (copy_from_user(kernel_buffer, user_buffer, bytes_to_copy))
        return -EFAULT;

    kernel_buffer[bytes_to_copy] = '\0';

    if (bytes_to_copy > 0 && kernel_buffer[bytes_to_copy - 1] == '\n') {
        kernel_buffer[bytes_to_copy - 1] = '\0';
    }

    string_received_flag = 1;
    pr_info("task_driver: Received string -> %s. Press any key to trigger tasklet!\n", kernel_buffer);

    return count;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .write = dev_write,
    .release = dev_release,
};

static int __init task_init(void) {
    int ret;

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("task_driver: failed to register char device\n");
        return major_number;
    }
    pr_info("task_driver: registered with major number %d\n", major_number);

    print_thread_ts = kthread_run(print_thread_func, NULL, "task_print_thread");
    if (IS_ERR(print_thread_ts)) {
        pr_err("task_driver: failed to create kernel thread\n");
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(print_thread_ts);
    }

    ret = request_irq(KBD_IRQ, kbd_irq_handler, IRQF_SHARED, DEVICE_NAME, (void *)kbd_irq_handler);
    if (ret) {
        pr_err("task_driver: failed to request IRQ %d\n", KBD_IRQ);
        kthread_stop(print_thread_ts);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    return 0;
}

static void __exit task_exit(void) {
    tasklet_kill(&my_tasklet);

    free_irq(KBD_IRQ, (void *)kbd_irq_handler);

    if (print_thread_ts) {
        kthread_stop(print_thread_ts);
    }

    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("task_driver: unloaded\n");
}

module_init(task_init);
module_exit(task_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Tasklet String Reversing Using Thread and IRQ");
