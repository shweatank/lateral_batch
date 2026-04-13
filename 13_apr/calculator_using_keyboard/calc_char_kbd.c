#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#define DEVICE_NAME "calc_char"
#define BUF_SIZE 256
#define KBD_IRQ 1
#define KBD_DATA_PORT 0x60

static int major_number;
static char kernel_buffer[BUF_SIZE];

static long operand_a = 0;
static long operand_b = 0;
static unsigned char last_scancode;

//KEYBOARD INTERRUPT HANDLERS 

static irqreturn_t kbd_irq_top(int irq, void *dev_id) {
    last_scancode = inb(KBD_DATA_PORT);
    return IRQ_WAKE_THREAD;
}

static irqreturn_t kbd_irq_thread(int irq, void *dev_id) {
    unsigned char scancode = last_scancode;
    long result = 0;
    
    if (scancode & 0x80) {
        return IRQ_HANDLED;
    }

    if (scancode == 0x1E) {         
        result = operand_a + operand_b;
        pr_info("calc_char_kbd: 'a' key pressed! Addition Result: %ld + %ld = %ld\n", 
                operand_a, operand_b, result);
    } else if (scancode == 0x1F) {  
        result = operand_a - operand_b;
        pr_info("calc_char_kbd: 's' key pressed! Subtraction Result: %ld - %ld = %ld\n", 
                operand_a, operand_b, result);
    } else if (scancode == 0x32) {  
        result = operand_a * operand_b;
        pr_info("calc_char_kbd: 'm' key pressed! Multiplication Result: %ld * %ld = %ld\n", 
                operand_a, operand_b, result);
    } else if (scancode == 0x20) {  
        if (operand_b != 0) {
            result = operand_a / operand_b;
            pr_info("calc_char_kbd: 'd' key pressed! Division Result: %ld / %ld = %ld\n", 
                    operand_a, operand_b, result);
        } else {
            pr_err("calc_char_kbd: 'd' key pressed! Error: Division by zero\n");
        }
    }

    return IRQ_HANDLED;
}

// --- FILE OPERATIONS ---

static int basic_open(struct inode *inode, struct file *file) {
    return 0;
}

static int basic_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t basic_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset) {
    int bytes_to_copy;
    char t1[32] = {0}, t2[32] = {0};
    int parsed;
    long a = 0, b = 0;

    bytes_to_copy = min(count, (size_t)(BUF_SIZE - 1));
    if (copy_from_user(kernel_buffer, user_buffer, bytes_to_copy))
        return -EFAULT;

    kernel_buffer[bytes_to_copy] = '\0';

    parsed = sscanf(kernel_buffer, "%31s %31s", t1, t2);
    if (parsed >= 2) {
        kstrtol(t1, 10, &a);
        kstrtol(t2, 10, &b);
        
        operand_a = a;
        operand_b = b;
        pr_info("calc_char_kbd: Received numbers: %ld, %ld. Waiting for 'a' (add), 's' (sub), 'm' (mul) or 'd' (div) on keyboard.\n", operand_a, operand_b);
    } else {
        pr_err("calc_char_kbd: Parsing failed. Please write two numbers, e.g. '3 4'\n");
    }

    return bytes_to_copy;
}

static struct file_operations basic_fops = {
    .owner = THIS_MODULE,
    .open = basic_open,
    .write = basic_write,
    .release = basic_release,
};


static int __init calc_char_kbd_init(void) {
    int ret;
    
    major_number = register_chrdev(0, DEVICE_NAME, &basic_fops);
    if (major_number < 0) {
        pr_err("calc_char_kbd: failed to register char device\n");
        return major_number;
    }
    pr_info("calc_char_kbd: loaded. major number = %d\n", major_number);

    ret = request_threaded_irq(KBD_IRQ, kbd_irq_top, kbd_irq_thread, IRQF_SHARED, "calc_char_kbd", (void *)kbd_irq_thread);
    if (ret) {
        pr_err("calc_char_kbd: Failed to register IRQ 1\n");
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    return 0;
}

static void __exit calc_char_kbd_exit(void) {
    free_irq(KBD_IRQ, (void *)kbd_irq_thread);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("calc_char_kbd: unloaded\n");
}

module_init(calc_char_kbd_init);
module_exit(calc_char_kbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Char Device + Keyboard Interrupt Hybrid Driver");
