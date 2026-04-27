#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/of.h>

#define DRIVER_NAME "my_uart0"
#define DEVICE_NAME "my_uart0"
#define BUFFER_SIZE 1024


/* PL011 registers offsets */
#define UART_BASE 0XFE201000
#define UART_DR     0x00    /* Data register */
#define UART_FR     0x18    /* Flag register */
#define UART_IBRD   0x24    /* Integer Baud Rate divisor */
#define UART_FBRD   0x28    // Fractional Baud Rate Divisor
#define UART_LCRH   0x2C
#define UART_CR     0x30
#define UART_IMSC   0x38
#define UART_ICR    0x44

/* flag bits */
#define FR_TXFF (1 << 5)
#define FR_RXFE (1 << 4)
#define FR_BUSY (1 << 3)

#define UART_LCRH_WLEN_8BIT (0x3 << 5)
#define UART_LCRH_FEN (1 << 4)


#define CR_UARTEN (1 << 0)
#define CR_TXE    (1 << 8)
#define CR_RXE    (1 << 9)

static void __iomem *uart_base;
static int major;

static int uart_open(struct inode *inode, struct file *file)
{
	pr_info("rpi_uart : Device opened\n");
	return 0;
}

static int uart_release(struct inode *inode, struct file *file)
{
	pr_info("rpi_uart: Device closed\n");
	return 0;
}

static ssize_t uart_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	size_t i = 0;
	char tmp;

	pr_info("rpi_uart: Attempting to write %zu bytes\n", count);
	while (i < count) {
		if (readl(uart_base + UART_FR) & FR_TXFF) {
			pr_info("rpi_uart: TX FIFO full\n");
			if (i == 0) 
				break;
			else
				return i;
		}
		if (copy_from_user(&tmp, buf + i, 1)) {
			pr_err("rpi_uart: copy_from_user failed\n");
			return -EFAULT;
		}
		pr_info("rpi_uart: Writing byte 0x%02x\n", tmp);
		writel(tmp,uart_base + UART_DR);
		/* Wait for transmission to complete */
		while (readl(uart_base + UART_FR) & FR_BUSY)
			cpu_relax();
		i++;
	}
	pr_info("rpi_uart: Wrote %zu bytes\n", i);
	return i;
}

static ssize_t uart_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	size_t i=0;
	char tmp;
	pr_info("enter read uatr : \n");
	while(i < count){
		if(readl(uart_base + UART_FR) & FR_RXFE){
			if(i==0){
				break;
			}
			else{
				return i;
			}
		}
		tmp=(char)(readl(uart_base + UART_DR)&0xFF);
		if(copy_to_user(buf + i, &tmp, 1)){
			pr_err("copy to user failed\n");
			return -EFAULT;
		}
		i++;
	}
	return i;

}

static struct file_operations uart_fops = {
	.owner = THIS_MODULE,
	.open = uart_open,
	.release = uart_release,
	.read = uart_read,
	.write = uart_write,
};

static void rpi_uart_hw_init(void)
{
	/* Disable interrupts */
	writel(0, uart_base + UART_IMSC);

	/* Clear interrupt flags */
	writel(0x7FF, uart_base + UART_ICR);

	/* Disable UART */
	writel(0, uart_base + UART_CR);

	/* Set baud rate to 115200 */
	writel(26, uart_base + UART_IBRD);
	writel(3, uart_base + UART_FBRD);

	/* 8-bit */
	writel((3 << 5), uart_base + UART_LCRH);

	writel(UART_LCRH_WLEN_8BIT | UART_LCRH_FEN, uart_base + UART_LCRH);

	/* Enable UART, RX, TX */
	writel(CR_UARTEN | CR_TXE | CR_RXE, uart_base + UART_CR);
}

static int uart_probe(struct platform_device *pdev)
{
	struct resource *res;

	pr_info("my_uart0: probe called\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	uart_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(uart_base))
		return PTR_ERR(uart_base);

	/* Register char device */
	major = register_chrdev(0, DRIVER_NAME, &uart_fops);
	pr_info("my_uart0: registered char device with major %d\n", major);

	rpi_uart_hw_init();
	pr_info("init\n");

	return 0;
}

static int uart_remove(struct platform_device *pdev)
{
	unregister_chrdev(major, DRIVER_NAME);
        return 0;
}

static const struct of_device_id uart_dt_ids[] = {
	{ .compatible = "myproject,my-uart0" },  // <--- DT match
	{}
};
MODULE_DEVICE_TABLE(of, uart_dt_ids);

static struct platform_driver uart_driver = {
	.probe = uart_probe,
	.remove = uart_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = uart_dt_ids,
	},
};

static int __init rpi_uart_module_init(void)
{
	return platform_driver_register(&uart_driver);
}

static void __exit rpi_uart_module_exit(void)
{
	platform_driver_unregister(&uart_driver);
}

module_init(rpi_uart_module_init);
module_exit(rpi_uart_module_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("UART0 driver for Raspberry Pi 4B (BCM2711)");
