#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>

#define IR_MAGIC        'i'
#define IR_GET_STATUS   _IOR(IR_MAGIC, 0, int)

#define UART_DEV "/dev/my_uart0"   
#define IR_DEV   "/dev/ir_sensor"

int main(void)
{
	int ir_fd, uart_fd;
	int status = 0;
	struct termios uart_cfg;

	/* Open IR sensor device */
	ir_fd = open(IR_DEV, O_RDWR);
	if (ir_fd < 0) {
		perror("IR open");
		return 1;
	}

	/* Open UART */
	uart_fd = open(UART_DEV, O_RDWR | O_NOCTTY);
	if (uart_fd < 0) {
		perror("UART open");
		close(ir_fd);
		return 1;
	}

	printf("Monitoring IR sensor...\n");
	int prev_status = 0;

	while (1) {
		ioctl(ir_fd, IR_GET_STATUS, &status);
		if (status != prev_status) {
			printf("Status=%d\n",status);
			char msg = status ? '1' : '0';
			write(uart_fd, &msg, 1);
			msg = '0';
		}

		prev_status = status;
		usleep(30000);
	}

	close(uart_fd);
	close(ir_fd);
	return 0;
}
