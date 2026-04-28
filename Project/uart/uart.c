	#include <linux/module.h>
	#include <linux/platform_device.h>
	#include <linux/of.h>
	#include <linux/of_address.h>
	#include <linux/fs.h>
	#include <linux/uaccess.h>
	#include <linux/io.h>
	#include <linux/interrupt.h>
	#include <linux/wait.h>
	#include <linux/workqueue.h>
	#include <linux/mutex.h>

	#define DRIVER_NAME "my_uart0"
	#define DEVICE_NAME "my_uart0"

	#define UART_DR     0x00
	#define UART_FR     0x18
	#define UART_IBRD   0x24
	#define UART_FBRD   0x28
	#define UART_LCRH   0x2C
	#define UART_CR     0x30
	#define UART_IMSC   0x38
	#define UART_MIS    0x40
	#define UART_ICR    0x44

	#define FR_TXFF (1 << 5)
	#define FR_RXFE (1 << 4)
	#define FR_BUSY (1 << 3)

	#define UART_LCRH_WLEN_8BIT (0x3 << 5)
	#define UART_LCRH_FEN       (1 << 4)

	#define CR_UARTEN (1 << 0)
	#define CR_TXE    (1 << 8)
	#define CR_RXE    (1 << 9)

	/* PL011 interrupt bits */
	#define UART_RX_INTR   (1 << 4)
	#define UART_RT_INTR   (1 << 6)

	/* ioctl commands */
	#define UART_IOCTL_MAGIC      'u'
	#define UART_IOCTL_CLEAR_RX   _IO(UART_IOCTL_MAGIC, 1)
	#define UART_IOCTL_SET_BAUD   _IOW(UART_IOCTL_MAGIC, 2, int)

	#define RX_BUF_SIZE 1024

	static void __iomem *uart_base;
	static int major;
	static int irq_num;

	/* Waitqueue */
	static DECLARE_WAIT_QUEUE_HEAD(rx_wait_queue);

	/* Workqueue */
	static struct workqueue_struct *uart_wq;
	static DECLARE_WORK(uart_rx_work, NULL);

	/* RX software buffer */
	static char rx_buffer[RX_BUF_SIZE];
	static int rx_head;
	static int rx_tail;

	static DEFINE_MUTEX(rx_lock);

	static int rx_buffer_empty(void)
	{
		return rx_head == rx_tail;
	}

	static int rx_buffer_full(void)
	{
		return ((rx_head + 1) % RX_BUF_SIZE) == rx_tail;
	}

	static void rx_buffer_put(char c)
	{
		if (!rx_buffer_full()) {
			rx_buffer[rx_head] = c;
			rx_head = (rx_head + 1) % RX_BUF_SIZE;
		}
	}

	static int rx_buffer_get(char *c)
	{
		if (rx_buffer_empty())
			return -1;

		*c = rx_buffer[rx_tail];
		rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
		return 0;
	}

	/* Workqueue function */
	static void uart_rx_work_func(struct work_struct *work)
	{
		char data;
		pr_info("WORKQUEUE EXECUTED\n");  //


		mutex_lock(&rx_lock);

		while (!(readl(uart_base + UART_FR) & FR_RXFE)) {
			data = readl(uart_base + UART_DR) & 0xFF;
			rx_buffer_put(data);
		}

		mutex_unlock(&rx_lock);

		/* Wake up user read() */
		wake_up_interruptible(&rx_wait_queue);
	}

	/* Interrupt handler */
	static irqreturn_t uart_irq_handler(int irq, void *dev_id)
	{
		u32 status;
		pr_info("INTERRUPT OCCURRED\n");   //

		status = readl(uart_base + UART_MIS);

		if (status & (UART_RX_INTR | UART_RT_INTR)) {
			/* Clear interrupt */
			writel(UART_RX_INTR | UART_RT_INTR, uart_base + UART_ICR);

			/* Do heavy work in workqueue */
			queue_work(uart_wq, &uart_rx_work);

			return IRQ_HANDLED;
		}

		return IRQ_NONE;
	}

	static int uart_open(struct inode *inode, struct file *file)
	{
		pr_info("my_uart0: device opened\n");
		return 0;
	}

	static int uart_release(struct inode *inode, struct file *file)
	{
		pr_info("my_uart0: device closed\n");
		return 0;
	}

	static ssize_t uart_write(struct file *file,
				  const char __user *buf,
				  size_t count,
				  loff_t *offset)
	{
		size_t i;
		char tmp;

		for (i = 0; i < count; i++) {
			if (copy_from_user(&tmp, buf + i, 1))
				return -EFAULT;

			while (readl(uart_base + UART_FR) & FR_TXFF)
				cpu_relax();

			writel(tmp, uart_base + UART_DR);

			while (readl(uart_base + UART_FR) & FR_BUSY)
				cpu_relax();
		}

		return count;
	}

	static ssize_t uart_read(struct file *file,
				 char __user *buf,
				 size_t count,
				 loff_t *offset)
	{
		size_t i = 0;
		char tmp;
              pr_info("READ: Going to sleep\n"); 
		/* Wait until RX buffer has data */
		if (wait_event_interruptible(rx_wait_queue, !rx_buffer_empty()))
			return -ERESTARTSYS;

		mutex_lock(&rx_lock);
		pr_info("READ: Woken up\n");

		while (i < count) {
			if (rx_buffer_get(&tmp) < 0)
				break;

			if (copy_to_user(buf + i, &tmp, 1)) {
				mutex_unlock(&rx_lock);
				return -EFAULT;
			}

			i++;
		}

		mutex_unlock(&rx_lock);

		return i;
	}

	/* ioctl function */
	static long uart_ioctl(struct file *file,
			       unsigned int cmd,
			       unsigned long arg)
	{
		int baud;

		switch (cmd) {

		case UART_IOCTL_CLEAR_RX:
			mutex_lock(&rx_lock);
			rx_head = 0;
			rx_tail = 0;
			mutex_unlock(&rx_lock);
			pr_info("my_uart0: RX buffer cleared\n");
			break;

		case UART_IOCTL_SET_BAUD:
			if (copy_from_user(&baud, (int __user *)arg, sizeof(baud)))
				return -EFAULT;

			pr_info("my_uart0: requested baud = %d\n", baud);

			/*
			 * Example only:
			 * Real baud setting requires UART clock calculation.
			 */
			if (baud == 115200) {
				writel(26, uart_base + UART_IBRD);
				writel(3, uart_base + UART_FBRD);
			} else {
				pr_err("my_uart0: unsupported baud rate\n");
				return -EINVAL;
			}
			break;

		default:
			return -EINVAL;
		}

		return 0;
	}

	static struct file_operations uart_fops = {
		.owner          = THIS_MODULE,
		.open           = uart_open,
		.release        = uart_release,
		.read           = uart_read,
		.write          = uart_write,
		.unlocked_ioctl = uart_ioctl,
	};

	static void rpi_uart_hw_init(void)
	{
		/* Disable UART */
		writel(0, uart_base + UART_CR);

		/* Disable interrupts */
		writel(0, uart_base + UART_IMSC);

		/* Clear interrupts */
		writel(0x7FF, uart_base + UART_ICR);

		/* Baud rate: 115200 */
		writel(26, uart_base + UART_IBRD);
		writel(3, uart_base + UART_FBRD);

		/* 8-bit + FIFO enable */
		writel(UART_LCRH_WLEN_8BIT | UART_LCRH_FEN,
		       uart_base + UART_LCRH);

		/* Enable RX interrupt and RX timeout interrupt */
		writel(UART_RX_INTR | UART_RT_INTR,
		       uart_base + UART_IMSC);

		/* Enable UART, TX, RX */
		writel(CR_UARTEN | CR_TXE | CR_RXE,
		       uart_base + UART_CR);
	}

	static int uart_probe(struct platform_device *pdev)
	{
		struct resource *res;
		int ret;

		pr_info("my_uart0: probe called\n");

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		uart_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(uart_base))
			return PTR_ERR(uart_base);

		irq_num = platform_get_irq(pdev, 0);
		if (irq_num < 0) {
			pr_err("my_uart0: failed to get IRQ\n");
			return irq_num;
		}

		major = register_chrdev(0, DRIVER_NAME, &uart_fops);
		if (major < 0) {
			pr_err("my_uart0: register_chrdev failed\n");
			return major;
		}

		uart_wq = create_singlethread_workqueue("uart_rx_wq");
		if (!uart_wq) {
			unregister_chrdev(major, DRIVER_NAME);
			return -ENOMEM;
		}

		INIT_WORK(&uart_rx_work, uart_rx_work_func);

		ret = request_irq(irq_num,
				  uart_irq_handler,
				  0,
				  DRIVER_NAME,
				  NULL);
		if (ret) {
			pr_err("my_uart0: request_irq failed\n");
			destroy_workqueue(uart_wq);
			unregister_chrdev(major, DRIVER_NAME);
			return ret;
		}

		rpi_uart_hw_init();

		pr_info("my_uart0: registdgaered with major %d, irq %d\n",
			major, irq_num);

		return 0;
	}

	static int uart_remove(struct platform_device *pdev)
	{
		writel(0, uart_base + UART_IMSC);
		writel(0, uart_base + UART_CR);

		free_irq(irq_num, NULL);

		flush_workqueue(uart_wq);
		destroy_workqueue(uart_wq);

		unregister_chrdev(major, DRIVER_NAME);

		pr_info("my_uart0: removed\n");

		return 0;
	}

	static const struct of_device_id uart_dt_ids[] = {
		{ .compatible = "myproject,my-uart0" },
		{}
	};
	MODULE_DEVICE_TABLE(of, uart_dt_ids);

	static struct platform_driver uart_driver = {
		.probe  = uart_probe,
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
	MODULE_DESCRIPTION("UART0 driver with interrupt, waitqueue, workqueue and ioctl");
