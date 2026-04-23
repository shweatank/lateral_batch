#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#define I2C_BUS_AVAILABLE   1
#define SLAVE_DEVICE_NAME   "ETX_OLED"
#define SSD1306_SLAVE_ADDR  0x3C

static struct i2c_adapter *etx_i2c_adapter = NULL;
static struct i2c_client  *etx_i2c_client_oled = NULL;

static int I2C_Write(struct i2c_client *client, unsigned char *buf, unsigned int len)
{
    int ret = i2c_master_send(client, buf, len);
    if (ret < 0)
        pr_err("I2C write failed\n");
    return ret;
}

static void SSD1306_Write(struct i2c_client *client, bool is_cmd, unsigned char data)
{
    unsigned char buf[2];
    buf[0] = is_cmd ? 0x00 : 0x40;
    buf[1] = data;
    I2C_Write(client, buf, 2);
    udelay(10);
}

static void SSD1306_SetCursor(struct i2c_client *client, unsigned char page, unsigned char column)
{
    SSD1306_Write(client, true, 0xB0 + page);
    SSD1306_Write(client, true, column & 0x0F);
    SSD1306_Write(client, true, 0x10 | (column >> 4));
}

static void SSD1306_Fill(struct i2c_client *client, unsigned char data)
{
    int i;
    SSD1306_SetCursor(client, 0, 0);
    for (i = 0; i < (128 * 4); i++)
        SSD1306_Write(client, false, data);
}

static int SSD1306_DisplayInit(struct i2c_client *client)
{
    msleep(100);
    SSD1306_Write(client, true, 0xAE);
    SSD1306_Write(client, true, 0xD5);
    SSD1306_Write(client, true, 0x80);
    SSD1306_Write(client, true, 0xA8);
    SSD1306_Write(client, true, 0x1F);
    SSD1306_Write(client, true, 0xD3);
    SSD1306_Write(client, true, 0x00);
    SSD1306_Write(client, true, 0x40);
    SSD1306_Write(client, true, 0x8D);
    SSD1306_Write(client, true, 0x14);
    SSD1306_Write(client, true, 0x20);
    SSD1306_Write(client, true, 0x00);
    SSD1306_Write(client, true, 0xA1);
    SSD1306_Write(client, true, 0xC8);
    SSD1306_Write(client, true, 0xDA);
    SSD1306_Write(client, true, 0x02);
    SSD1306_Write(client, true, 0x81);
    SSD1306_Write(client, true, 0x8F);
    SSD1306_Write(client, true, 0xD9);
    SSD1306_Write(client, true, 0xF1);
    SSD1306_Write(client, true, 0xDB);
    SSD1306_Write(client, true, 0x40);
    SSD1306_Write(client, true, 0xA4);
    SSD1306_Write(client, true, 0xA6);
    SSD1306_Write(client, true, 0xAF);
    return 0;
}

static void SSD1306_PrintJumboChar(struct i2c_client *client, char c, unsigned char start_col)
{
    int i, p, x;
    const unsigned char *p_font;
    unsigned char scaled[4];
    unsigned char b;

    static const unsigned char font_U[] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
    static const unsigned char font_B[] = {0x7F, 0x49, 0x49, 0x49, 0x36};
    static const unsigned char font_R[] = {0x7F, 0x09, 0x19, 0x29, 0x46};
    static const unsigned char space[]  = {0x00, 0x00, 0x00, 0x00, 0x00};

    switch(c) {
        case 'U': p_font = font_U; break;
        case 'B': p_font = font_B; break;
        case 'R': p_font = font_R; break;
        case ' ': p_font = space; break;
        default: return;
    }

    for (i = 0; i < 5; i++) {
        b = p_font[i];
        scaled[0] = ((b & 0x01) ? 0x0F : 0x00) | ((b & 0x02) ? 0xF0 : 0x00);
        scaled[1] = ((b & 0x04) ? 0x0F : 0x00) | ((b & 0x08) ? 0xF0 : 0x00);
        scaled[2] = ((b & 0x10) ? 0x0F : 0x00) | ((b & 0x20) ? 0xF0 : 0x00);
        scaled[3] = ((b & 0x40) ? 0x0F : 0x00) | ((b & 0x80) ? 0xF0 : 0x00);

        for (x = 0; x < 4; x++) {
            for (p = 0; p < 4; p++) {
                SSD1306_SetCursor(client, p, start_col + (i * 4) + x);
                SSD1306_Write(client, false, scaled[p]);
            }
        }
    }
}

static void SSD1306_PrintJumboString(struct i2c_client *client, const char *str)
{
    int col = 10;
    while (*str) {
        SSD1306_PrintJumboChar(client, *str++, col);
        col += 24;
    }
}

static int etx_oled_probe(struct i2c_client *client)
{
    SSD1306_DisplayInit(client);
    SSD1306_Fill(client, 0x00);
    SSD1306_PrintJumboString(client, "UBR");
    pr_info("OLED: UBR displayed\n");
    return 0;
}

static void etx_oled_remove(struct i2c_client *client)
{
    SSD1306_Fill(client, 0x00);
    pr_info("OLED removed\n");
}

static struct i2c_device_id etx_oled_id[] = {
    { SLAVE_DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, etx_oled_id);

static struct i2c_driver etx_oled_driver = {
    .driver = {
        .name = SLAVE_DEVICE_NAME,
    },
    .probe  = etx_oled_probe,
    .remove = etx_oled_remove,
    .id_table = etx_oled_id,
};

static struct i2c_board_info oled_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, SSD1306_SLAVE_ADDR)
};

static int __init etx_driver_init(void)
{
    etx_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);
    if (!etx_i2c_adapter)
        return -ENODEV;

    etx_i2c_client_oled = i2c_new_client_device(etx_i2c_adapter, &oled_i2c_board_info);
    i2c_add_driver(&etx_oled_driver);
    i2c_put_adapter(etx_i2c_adapter);

    pr_info("OLED driver loaded\n");
    return 0;
}

static void __exit etx_driver_exit(void)
{
    i2c_unregister_device(etx_i2c_client_oled);
    i2c_del_driver(&etx_oled_driver);
    pr_info("OLED driver removed\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("print UBR on i2c display");