// spi_ili9225.c
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "font8x8_basic.h"

#define DRIVER_NAME   "ili9225"
#define CLASS_NAME    "ili"
#define DEVICE_NAME   "ili9225_char"

#define LINE_HEIGHT   16      /* 8px font * 2x scale */
#define MARGIN_Y      20
#define SCREEN_WIDTH  176
#define SCREEN_HEIGHT 220

/* ---------------------------------------------------- */
/* Globals                                               */
/* ---------------------------------------------------- */
static dev_t        dev_num;
static struct class *ili_class;
static struct cdev  ili_cdev;

struct ili9225 {
    struct spi_device *spi;
    struct gpio_desc  *dc;
    struct gpio_desc  *reset;
};

static struct ili9225 *g_lcd;
static int cursor_x = 10;
static int cursor_y = MARGIN_Y;
static u16 text_color = 0xF800;

/* ---------------------------------------------------- */
/* SPI helpers                                           */
/* ---------------------------------------------------- */
static int ili9225_write16(struct ili9225 *lcd, u16 value)
{
    u8 buf[2];
    buf[0] = value >> 8;
    buf[1] = value & 0xFF;
    return spi_write(lcd->spi, buf, 2);
}

static int ili9225_write_reg(struct ili9225 *lcd, u16 reg, u16 data)
{
    gpiod_set_value(lcd->dc, 0);
    ili9225_write16(lcd, reg);
    gpiod_set_value(lcd->dc, 1);
    return ili9225_write16(lcd, data);
}

/* ---------------------------------------------------- */
/* Reset                                                 */
/* ---------------------------------------------------- */
static void ili9225_reset(struct ili9225 *lcd)
{
    gpiod_set_value(lcd->reset, 1);
    msleep(5);
    gpiod_set_value(lcd->reset, 0);
    msleep(20);
    gpiod_set_value(lcd->reset, 1);
    msleep(50);
}

/* ---------------------------------------------------- */
/* Init sequence                                         */
/* ---------------------------------------------------- */
static void ili9225_init(struct ili9225 *lcd)
{
    ili9225_reset(lcd);

    ili9225_write_reg(lcd, 0x0001, 0x011C);
    ili9225_write_reg(lcd, 0x0002, 0x0100);
    ili9225_write_reg(lcd, 0x0003, 0x1030);
    ili9225_write_reg(lcd, 0x0008, 0x0808);
    ili9225_write_reg(lcd, 0x000C, 0x0000);
    ili9225_write_reg(lcd, 0x000F, 0x0B01);

    ili9225_write_reg(lcd, 0x0010, 0x0A00);
    ili9225_write_reg(lcd, 0x0011, 0x1038);
    msleep(50);
    ili9225_write_reg(lcd, 0x0012, 0x1121);
    ili9225_write_reg(lcd, 0x0013, 0x0063);
    ili9225_write_reg(lcd, 0x0014, 0x5A00);
    msleep(50);

    ili9225_write_reg(lcd, 0x0007, 0x1017);
    msleep(20);
}

/* ---------------------------------------------------- */
/* Fill screen                                           */
/* ---------------------------------------------------- */
static void ili9225_fill(struct ili9225 *lcd, u16 color)
{
    int x, y;

    ili9225_write_reg(lcd, 0x0036, 175);
    ili9225_write_reg(lcd, 0x0037, 0);
    ili9225_write_reg(lcd, 0x0038, 219);
    ili9225_write_reg(lcd, 0x0039, 0);
    ili9225_write_reg(lcd, 0x0020, 0);
    ili9225_write_reg(lcd, 0x0021, 0);

    gpiod_set_value(lcd->dc, 0);
    ili9225_write16(lcd, 0x0022);
    gpiod_set_value(lcd->dc, 1);

    for (y = 0; y < SCREEN_HEIGHT; y++)
        for (x = 0; x < SCREEN_WIDTH; x++)
            ili9225_write16(lcd, color);
}

/* ---------------------------------------------------- */
/* Draw pixel                                            */
/* ---------------------------------------------------- */
static void drawPixel(int x, int y, u16 color)
{
    struct ili9225 *lcd = g_lcd;

    gpiod_set_value(lcd->dc, 0);
    ili9225_write16(lcd, 0x0020);
    gpiod_set_value(lcd->dc, 1);
    ili9225_write16(lcd, x);

    gpiod_set_value(lcd->dc, 0);
    ili9225_write16(lcd, 0x0021);
    gpiod_set_value(lcd->dc, 1);
    ili9225_write16(lcd, y);

    gpiod_set_value(lcd->dc, 0);
    ili9225_write16(lcd, 0x0022);
    gpiod_set_value(lcd->dc, 1);
    ili9225_write16(lcd, color);
}

/* ---------------------------------------------------- */
/* Draw character (2x scaled)                            */
/* ---------------------------------------------------- */
static void drawChar(int x, int y, char c, u16 color)
{
    const u8 *bitmap;
    int row, col;

    if (c < 32 || c > 127)
        return;

    bitmap = font8x8[c - 32];

    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            if (bitmap[row] & (1 << (7 - col))) {
                drawPixel(x + col * 2,     y + row * 2,     color);
                drawPixel(x + col * 2 + 1, y + row * 2,     color);
                drawPixel(x + col * 2,     y + row * 2 + 1, color);
                drawPixel(x + col * 2 + 1, y + row * 2 + 1, color);
            } else {
                drawPixel(x + col * 2,     y + row * 2,     0xFFFF);
                drawPixel(x + col * 2 + 1, y + row * 2,     0xFFFF);
                drawPixel(x + col * 2,     y + row * 2 + 1, 0xFFFF);
                drawPixel(x + col * 2 + 1, y + row * 2 + 1, 0xFFFF);
            }
        }
    }
}

/* ---------------------------------------------------- */
/* char device write                                     */
/* ---------------------------------------------------- */
static ssize_t ili_write(struct file *file,
                         const char __user *buf,
                         size_t len,
                         loff_t *off)
{
    char *kbuf;
    int i;
    char cmd[16];

    if (len > 4096)
        len = 4096;

    kbuf = kmalloc(len + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, buf, len)) {
        kfree(kbuf);
        return -EFAULT;
    }
    kbuf[len] = '\0';

    if (len <= 10) {
        int cmd_len = len;
        strncpy(cmd, kbuf, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        if (cmd_len > 0 && cmd[cmd_len - 1] == '\n') {
            cmd[cmd_len - 1] = '\0';
        }

        if (strcmp(cmd, "clear") == 0) {
            ili9225_fill(g_lcd, 0xFFFF);
            cursor_x = 10;
            cursor_y = MARGIN_Y;
            kfree(kbuf);
            return len;
        } else if (strcmp(cmd, "red") == 0) {
            text_color = 0xF800;
            kfree(kbuf);
            return len;
        } else if (strcmp(cmd, "green") == 0) {
            text_color = 0x07E0;
            kfree(kbuf);
            return len;
        } else if (strcmp(cmd, "blue") == 0) {
            text_color = 0x001F;
            kfree(kbuf);
            return len;
        }
    }

    /* Process text output */
    for (i = 0; i < len; i++) {
        if (kbuf[i] == '\n') {
            cursor_x = 10;
            cursor_y += LINE_HEIGHT;
            if (cursor_y + LINE_HEIGHT > SCREEN_HEIGHT) {
                cursor_y = MARGIN_Y;
            }
        } else if (kbuf[i] >= 32 && kbuf[i] <= 127) {
            if (cursor_x + 16 > SCREEN_WIDTH) {
                cursor_x = 10;
                cursor_y += LINE_HEIGHT;
                if (cursor_y + LINE_HEIGHT > SCREEN_HEIGHT) {
                    cursor_y = MARGIN_Y;
                }
            }
            drawChar(cursor_x, cursor_y, kbuf[i], text_color);
            cursor_x += 16;
        }
    }

    kfree(kbuf);
    return len;
}

/* ---------------------------------------------------- */
/* File operations                                       */
/* ---------------------------------------------------- */
static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = ili_write,
};

/* ---------------------------------------------------- */
/* Probe                                                 */
/* ---------------------------------------------------- */
static int ili9225_probe(struct spi_device *spi)
{
    struct ili9225 *lcd;
    int ret;

    lcd = devm_kzalloc(&spi->dev, sizeof(*lcd), GFP_KERNEL);
    if (!lcd)
        return -ENOMEM;

    lcd->spi = spi;
    spi_set_drvdata(spi, lcd);

    lcd->dc = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_LOW);
    if (IS_ERR(lcd->dc)) {
        dev_err(&spi->dev, "Failed to get DC gpio\n");
        return PTR_ERR(lcd->dc);
    }

    lcd->reset = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(lcd->reset)) {
        dev_err(&spi->dev, "Failed to get RESET gpio\n");
        return PTR_ERR(lcd->reset);
    }

    spi->mode          = SPI_MODE_0;
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret) {
        dev_err(&spi->dev, "SPI setup failed: %d\n", ret);
        return ret;
    }

    ili9225_init(lcd);
    ili9225_fill(lcd, 0xFFFF);  /* white screen on boot */

    g_lcd    = lcd;
    cursor_x = 10;
    cursor_y = MARGIN_Y;
    text_color = 0xF800;

    /* Register char device */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret) {
        dev_err(&spi->dev, "alloc_chrdev_region failed\n");
        return ret;
    }

    cdev_init(&ili_cdev, &fops);
    ret = cdev_add(&ili_cdev, dev_num, 1);
    if (ret) {
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    ili_class = class_create(CLASS_NAME);
    if (IS_ERR(ili_class)) {
        cdev_del(&ili_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(ili_class);
    }

    device_create(ili_class, NULL, dev_num, NULL, DEVICE_NAME);

    dev_info(&spi->dev, "ILI9225 LCD initialized — /dev/%s ready\n", DEVICE_NAME);
    return 0;
}

/* ---------------------------------------------------- */
/* Remove                                                */
/* ---------------------------------------------------- */
static void ili9225_remove(struct spi_device *spi)
{
    device_destroy(ili_class, dev_num);
    class_destroy(ili_class);
    cdev_del(&ili_cdev);
    unregister_chrdev_region(dev_num, 1);
    dev_info(&spi->dev, "ILI9225 removed\n");
}

/* ---------------------------------------------------- */
/* Device Tree match                                     */
/* ---------------------------------------------------- */
static const struct of_device_id ili9225_dt_ids[] = {
    { .compatible = "ilitek,ili9225" },
    { }
};
MODULE_DEVICE_TABLE(of, ili9225_dt_ids);

/* ---------------------------------------------------- */
/* SPI driver struct                                     */
/* ---------------------------------------------------- */
static struct spi_driver ili9225_driver = {
    .driver = {
        .name           = DRIVER_NAME,
        .of_match_table = ili9225_dt_ids,
    },
    .probe  = ili9225_probe,
    .remove = ili9225_remove,
};

module_spi_driver(ili9225_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("ILI9225 SPI LCD Driver with line scroll and clear");