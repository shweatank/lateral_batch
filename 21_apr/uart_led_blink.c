#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kfifo.h>

#define DEVICE_NAME "uart_drv"
#define CLASS_NAME "uart_class"


#define PERIPHERAL_BASE 0xfe000000
#define GPIO_BASE (PERIPHERAL_BASE + 0x200000)
#define AUX_BASE  (PERIPHERAL_BASE + 0x215000)


#define GPFSEL1    0x04
#define GPSET0     0x1c
#define GPCLR0     0x28

#define AUX_ENABLES     0x04
#define AUX_MU_IO_REG   0x40
#define AUX_MU_IER_REG  0x44
#define AUX_MU_IIR_REG  0x48
#define AUX_MU_LCR_REG  0x4c
#define AUX_MU_LSR_REG  0x54
#define AUX_MU_CNTL_REG 0x60
#define AUX_MU_BAUD_REG 0x68

#define FIFO_SIZE 1024

static void __iomem *gpio_reg_base;
static void __iomem *aux_reg_base;

static dev_t dev_num;
static struct cdev uart_cdev;
static struct class *uart_class;

static struct kfifo rx_fifo;
static wait_queue_head_t read_wait;
static int irq_num = 28;

static irqreturn_t uart_irq_handler(int irq, void *dev_id) {
    uint32_t iir, status;
    char data;

    iir = readl(aux_reg_base + AUX_MU_IIR_REG);
    
    if (!(iir & 0x01) && ((iir & 0x06) == 0x04)) {
        while (readl(aux_reg_base + AUX_MU_LSR_REG) & 0x01) {
            data = (char)(readl(aux_reg_base + AUX_MU_IO_REG) & 0xFF);
            kfifo_put(&rx_fifo, data);

            if (data == '1') {
                writel(1 << 17, gpio_reg_base + GPSET0);
                pr_info("UART Driver: LED ON (GPIO 17)\n");
            } else if (data == '0') {
                writel(1 << 17, gpio_reg_base + GPCLR0);
                pr_info("UART Driver: LED OFF (GPIO 17)\n");
            }
        }
        wake_up_interruptible(&read_wait);
        return IRQ_HANDLED;
    }

    return IRQ_NONE;
}

static int uart_open(struct inode *inode, struct file *file) {
    return 0;
}

static int uart_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t uart_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    int ret;
    unsigned int copied;

    if (kfifo_is_empty(&rx_fifo)) {
        if (file->f_flags & O_NONBLOCK) return -EAGAIN;
        ret = wait_event_interruptible(read_wait, !kfifo_is_empty(&rx_fifo));
        if (ret) return ret;
    }

    ret = kfifo_to_user(&rx_fifo, buf, len, &copied);
    return ret ? ret : copied;
}

static ssize_t uart_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    int i;
    char data;

    for (i = 0; i < len; i++) {
        if (copy_from_user(&data, buf + i, 1)) return -EFAULT;

        while (!(readl(aux_reg_base + AUX_MU_LSR_REG) & 0x20));

        writel(data, aux_reg_base + AUX_MU_IO_REG);
    }

    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = uart_open,
    .release = uart_release,
    .read = uart_read,
    .write = uart_write,
};

static int __init uart_driver_init(void) {
    uint32_t reg;

    pr_info("UART Driver: Initializing with Interrupts...\n");

    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) return -1;
    uart_class = class_create(CLASS_NAME);
    device_create(uart_class, NULL, dev_num, NULL, DEVICE_NAME);
    cdev_init(&uart_cdev, &fops);
    cdev_add(&uart_cdev, dev_num, 1);

    init_waitqueue_head(&read_wait);
    if (kfifo_alloc(&rx_fifo, FIFO_SIZE, GFP_KERNEL)) return -ENOMEM;

    gpio_reg_base = ioremap(GPIO_BASE, 0x100);
    aux_reg_base = ioremap(AUX_BASE, 0x100);

    reg = readl(gpio_reg_base + GPFSEL1);
    reg &= ~(7 << 12); 
    reg |= (2 << 12);  
    reg &= ~(7 << 15); 
    reg |= (2 << 15);  
    reg &= ~(7 << 21); 
    reg |= (1 << 21);  
    writel(reg, gpio_reg_base + GPFSEL1);

    writel(1, aux_reg_base + AUX_ENABLES);
    writel(0, aux_reg_base + AUX_MU_CNTL_REG);
    writel(3, aux_reg_base + AUX_MU_LCR_REG);  
    writel(270, aux_reg_base + AUX_MU_BAUD_REG); 
    writel(1, aux_reg_base + AUX_MU_IER_REG); 
    

    if (request_irq(irq_num, uart_irq_handler, IRQF_SHARED, DEVICE_NAME, (void *)(uart_irq_handler))) {
        pr_err("UART Driver: Cannot register IRQ %d\n", irq_num);
    }

    writel(3, aux_reg_base + AUX_MU_CNTL_REG); 

    pr_info("UART Driver: Ready\n");
    return 0;
}

static void __exit uart_driver_exit(void) {
    free_irq(irq_num, (void *)(uart_irq_handler));
    writel(0, aux_reg_base + AUX_ENABLES);
    iounmap(gpio_reg_base);
    iounmap(aux_reg_base);
    kfifo_free(&rx_fifo);
    cdev_del(&uart_cdev);
    device_destroy(uart_class, dev_num);
    class_destroy(uart_class);
    unregister_chrdev_region(dev_num, 1);
    pr_info("UART Driver: Unloaded\n");
}

module_init(uart_driver_init);
module_exit(uart_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("UART led blinking Driver for RPi4");
