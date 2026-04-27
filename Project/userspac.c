#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <errno.h>

#define UART_DEV   "/dev/my_uart0"
#define LCD_DEV    "/dev/ili9225_char"
#define IMAGE_FILE "/home/dell/Project/obj.rgb565"

#define LCD_W 176
#define LCD_H 220
#define FRAME_SIZE (LCD_W * LCD_H * 2)

/* Clear LCD */
static int lcd_clear(int lcd_fd)
{
	unsigned char *buf = calloc(1, FRAME_SIZE);
	if (!buf)
		return -1;

	ssize_t ret = write(lcd_fd, buf, FRAME_SIZE);
	free(buf);

	return (ret == FRAME_SIZE) ? 0 : -1;
}

/* Send RGB565 image to LCD */
static int lcd_show_image(int lcd_fd)
{
	int img_fd;
	unsigned char *buf;
	ssize_t rd, wr;

	img_fd = open(IMAGE_FILE, O_RDONLY);
	if (img_fd < 0) {
		perror("open image");
		return -1;
	}

	buf = malloc(FRAME_SIZE);
	if (!buf) {
		close(img_fd);
		return -1;
	}

	rd = read(img_fd, buf, FRAME_SIZE);
	close(img_fd);

	if (rd != FRAME_SIZE) {
		printf("Error: Image size mismatch: %zd bytes\n", rd);
		free(buf);
		return -1;
	}

	wr = write(lcd_fd, buf, FRAME_SIZE);
	free(buf);

	return (wr == FRAME_SIZE) ? 0 : -1;
}

int main(void)
{
	int uart_fd, lcd_fd;

	uart_fd = open(UART_DEV, O_RDONLY | O_NOCTTY);
	if (uart_fd < 0) {
		perror("open uart");
		return 1;
	}


	lcd_fd = open(LCD_DEV, O_WRONLY);
	if (lcd_fd < 0) {
		perror("open lcd");
		close(uart_fd);
		return 1;
	}

	printf("UART → LCD app started\n");
	printf("Send '0' to clear screen, '1' to show image\n");


	char ch;
	char last_cmd = -1;   
	int count = 0;

	while (1) {
		ssize_t r = read(uart_fd, &ch, 1);
		if (r <= 0) {
			usleep(10000);
			continue;
		}

		if (ch != '0' && ch != '1') {
			printf("Invalid data: 0x%02x\n", ch);
			continue;
		}

		// Ignore duplicate commands
		if (ch == last_cmd)
			continue;

		if (ch == '1') {
			printf("UART='1' → Display image (%d)\n", ++count);
			lcd_show_image(lcd_fd);
		} 
		else {
			printf("UART='0' → Clear LCD (%d)\n", ++count);
			lcd_clear(lcd_fd);
		}

		last_cmd = ch;
	}

	close(uart_fd);
	close(lcd_fd);
	return 0;
}
