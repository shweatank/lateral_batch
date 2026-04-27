#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>

#define DRIVER_NAME "ili9225"
#define DEVICE_NAME "ili9225_char"

static dev_t devt;
static struct cdev ili_cdev;
static struct class *ili_class;
static struct spi_device *ili9225_spi;

struct ili9225 {
	struct spi_device *spi;
	struct gpio_desc *reset;
	struct gpio_desc *dc;

	uint16_t pw; // panel width
	uint16_t ph; // panel height

	uint8_t *row_buf;
	size_t row_buf_size;

};

static inline int ili9225_write_cmd(struct ili9225 *lcd, uint8_t cmd)
{
	gpiod_set_value_cansleep(lcd->dc, 0); // command
	return spi_write(lcd->spi, &cmd, 1);
}

static inline int ili9225_write_data16(struct ili9225 *lcd, uint16_t data)
{
    uint8_t buf[2] = { data >> 8, data & 0xFF };

    gpiod_set_value_cansleep(lcd->dc, 1);
    return spi_write(lcd->spi, buf, 2);
}

static int ili9225_write_reg(struct ili9225 *lcd, uint8_t reg, uint16_t val)
{
	int ret = ili9225_write_cmd(lcd, reg);
	if (ret)
		return ret;
	return ili9225_write_data16(lcd, val);
}

// Initialization sequence (RGB565)
static int ili9225_init(struct ili9225 *lcd)
{

	/* Hardware reset */
	if (lcd->reset) {
		gpiod_set_value_cansleep(lcd->reset, 1);
		msleep(10);

		gpiod_set_value_cansleep(lcd->reset, 0);
		msleep(50);

		gpiod_set_value_cansleep(lcd->reset, 1);
		msleep(120);
	}

	/* Initial sequence using ili9225_write_reg */

	ili9225_write_reg(lcd, 0x10, 0x0000);
	ili9225_write_reg(lcd, 0x11, 0x0000);
	ili9225_write_reg(lcd, 0x12, 0x0000);
	ili9225_write_reg(lcd, 0x13, 0x0000);
	ili9225_write_reg(lcd, 0x14, 0x0000);
	usleep_range(40000, 41000);

	ili9225_write_reg(lcd, 0x11, 0x0018);
	ili9225_write_reg(lcd, 0x12, 0x6121);
	ili9225_write_reg(lcd, 0x13, 0x006F);
	ili9225_write_reg(lcd, 0x14, 0x495F);
	ili9225_write_reg(lcd, 0x10, 0x0800);
	usleep_range(10000, 11000);

	ili9225_write_reg(lcd, 0x11, 0x103B);
	usleep_range(50000, 51000);

	/* Driver output control */

	ili9225_write_reg(lcd, 0x01, 0x011C);
	ili9225_write_reg(lcd, 0x02, 0x0100);
	ili9225_write_reg(lcd, 0x03, 0x1030);
	ili9225_write_reg(lcd, 0x07, 0x0000);

	/* Frame rate */

	ili9225_write_reg(lcd, 0x08, 0x0808);

	/* Entry mode */

	ili9225_write_reg(lcd, 0x0F, 0x0B01);
	ili9225_write_reg(lcd, 0x0B, 0x1100);

	/* Set GRAM area - Window */

	ili9225_write_reg(lcd, 0x30, 0x0000);
	ili9225_write_reg(lcd, 0x31, 0x00DB);
	ili9225_write_reg(lcd, 0x32, 0x0000);
	ili9225_write_reg(lcd, 0x33, 0x0000);
	ili9225_write_reg(lcd, 0x34, 0x00DB);
	ili9225_write_reg(lcd, 0x35, 0x0000);
	ili9225_write_reg(lcd, 0x36, 0x00AF); //x-end
	ili9225_write_reg(lcd, 0x37, 0x0000); //x-start
	ili9225_write_reg(lcd, 0x38, 0x00DB); //y-end
	ili9225_write_reg(lcd, 0x39, 0x0000); //y-start

	/* Display ON */

	ili9225_write_reg(lcd, 0x07, 0x1017);

	return 0;

}

static void ili9225_set_window(struct ili9225 *lcd, uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
	ili9225_write_reg(lcd, 0x36, xe);
	ili9225_write_reg(lcd, 0x37, xs);
	ili9225_write_reg(lcd, 0x38, ye);
	ili9225_write_reg(lcd, 0x39, ys);

	/* Set RAM address */

	ili9225_write_reg(lcd, 0x20, xs);
	ili9225_write_reg(lcd, 0x21, ys);
	ili9225_write_cmd(lcd, 0x22); // RAM write

}

// Display function for RGB565 frame

static void ili9225_display_rgb565(struct ili9225 *lcd, uint8_t *frame565)
{

	uint16_t x, y;
	uint8_t *row_buf = lcd->row_buf;

	ili9225_set_window(lcd, 0, 0, lcd->pw - 1, lcd->ph - 1);

	gpiod_set_value_cansleep(lcd->dc, 1); // Data mode

	for (y = 0; y < lcd->ph; y++) {
		uint8_t *src = frame565 + (size_t)y * lcd->pw * 2;

		for (x = 0; x < lcd->pw; x++) {
			row_buf[2*x]     = src[2*x];     /* preserve byte order */
			row_buf[2*x + 1] = src[2*x + 1]; 
		}

		spi_write(lcd->spi, row_buf, lcd->row_buf_size);
	}

}


// File operations

static int ili9225_open(struct inode *inode, struct file *file)
{

	struct ili9225 *lcd = spi_get_drvdata(ili9225_spi);
	int ret;

	if (!lcd)
		return -ENODEV;

	/* initialize panel when device opened (optional) */

	ret = ili9225_init(lcd);

	if (ret) {
		dev_err(&lcd->spi->dev, "LCD init failed: %d\n", ret);
		return ret;
	}

	pr_info("ili9225: device opened\n");

	return 0;

}

static int ili9225_release(struct inode *inode, struct file *file)
{
	pr_info("ili9225: device closed\n");
	return 0;
}

// write function receives .rgb565 data from user-space

static ssize_t ili9225_write(struct file *file, const char __user *buf,size_t len, loff_t *off)
{

	struct ili9225 *lcd = spi_get_drvdata(ili9225_spi);
	uint8_t *kbuf;
	size_t expected_size;

	if (!lcd)
		return -ENODEV;

	expected_size = (size_t)lcd->pw * lcd->ph * 2;

	if (len != expected_size) { 
		pr_err("ili9225: expected %zu bytes, got %zu\n",
				expected_size, len);
		return -EINVAL;
	}

	kbuf = kmalloc(len, GFP_KERNEL);

	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, len)) {
		kfree(kbuf);
		return -EFAULT;
	}

	// Call display function
	ili9225_display_rgb565(lcd, kbuf);
	kfree(kbuf);

	return len;

}

static const struct file_operations ili9225_fops = {
	.owner   = THIS_MODULE,
	.open    = ili9225_open,
	.release = ili9225_release,
	.write   = ili9225_write,
};

static int ili9225_probe(struct spi_device *spi)
{
	struct ili9225 *lcd;
	int ret;

	pr_info(DRIVER_NAME ": probe called for %s\n", dev_name(&spi->dev));

	// Allocate driver private data
	lcd = devm_kzalloc(&spi->dev, sizeof(*lcd), GFP_KERNEL);
	if (!lcd)
		return -ENOMEM;

	lcd->spi = spi;
	lcd->pw = 176;
	lcd->ph = 220;
ili9225_spi = spi;
	/* ensure spi speed is set (read from DT if available, otherwise force) */

	uint32_t freq = 0;
	/* prefer device-tree value if present */

	if (!device_property_read_u32(&spi->dev, "spi-max-frequency", &freq)) {
		dev_info(&spi->dev, "DTS: using spi-max-frequency = %u\n", freq);
		spi->max_speed_hz = freq;
	}
	else {
		/* fallback safe default; tune as needed */
		spi->max_speed_hz = 10000000; /* 10 MHz */
	}

	/* apply the speed and mode to the controller */
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);

	if (ret) {
		dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
		return ret;
	}
	dev_info(&spi->dev, "SPI speed set to %u Hz\n", spi->max_speed_hz);

	lcd->row_buf_size = lcd->pw * 2;
	lcd->row_buf = devm_kzalloc(&spi->dev, lcd->row_buf_size, GFP_KERNEL);
	if (!lcd->row_buf){
		return -ENOMEM;}

	// Get GPIOs from device tree

	lcd->reset = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_HIGH);

	if (IS_ERR(lcd->reset)) {
		dev_err(&spi->dev, "Failed to get reset GPIO\n");
		return PTR_ERR(lcd->reset);
	}

	lcd->dc = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_LOW);

	if (IS_ERR(lcd->dc)) {
		dev_err(&spi->dev, "Failed to get dc GPIO\n");
		return PTR_ERR(lcd->dc);
	}

	// Save driver data

	spi_set_drvdata(spi, lcd);

	ret = ili9225_init(lcd);

	if (ret)
		dev_err(&lcd->spi->dev, "LCD init failed\n");


	/*Create /dev device */

	ret = alloc_chrdev_region(&devt, 0, 1, DEVICE_NAME);

	if (ret < 0)
		return ret;

	cdev_init(&ili_cdev, &ili9225_fops);
	ret = cdev_add(&ili_cdev, devt, 1);

	if (ret) {
		unregister_chrdev_region(devt, 1);
		return ret;
	}

	ili_class = class_create(DEVICE_NAME);
	if (IS_ERR(ili_class)) {
		cdev_del(&ili_cdev);
		unregister_chrdev_region(devt, 1);
		return PTR_ERR(ili_class);
	}

	device_create(ili_class, NULL, devt, NULL, DEVICE_NAME);
	pr_info(DRIVER_NAME ": /dev/%s created\n", DEVICE_NAME);

	return 0;

}

static void ili9225_remove(struct spi_device *spi)
{
	pr_info(DRIVER_NAME ": remove called\n");

	device_destroy(ili_class, devt);
	class_destroy(ili_class);

	cdev_del(&ili_cdev);
	unregister_chrdev_region(devt, 1);


ili9225_spi = NULL;
	spi_set_drvdata(spi, NULL);
	pr_info(DRIVER_NAME ": device removed\n");

}

static const struct of_device_id ili9225_of_match[] = {
	{ .compatible = "ilitek,ili9225" },
	{ }
};

MODULE_DEVICE_TABLE(of, ili9225_of_match);

static struct spi_driver ili9225_driver = {

	.driver = {
		.name   = "ili9225",
		.of_match_table = ili9225_of_match,

	},
	.probe  = ili9225_probe,
	.remove = ili9225_remove,
};

module_spi_driver(ili9225_driver);

MODULE_DESCRIPTION("Minimal ILI9225 SPI driver");
MODULE_LICENSE("GPL");
