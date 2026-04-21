#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define UART_DEV "/dev/uart_drv"

int main() {
    int fd;
    char choice;
    char cmd;

    fd = open(UART_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open UART device");
        return 1;
    }

    printf("UART LED Control\n");
    printf("1: LED ON\n");
    printf("0: LED OFF\n");
    printf("q: Quit\n");

    while (1) {
        printf("Enter choice: ");
        scanf(" %c", &choice);

        if (choice == 'q') break;

        if (choice == '1' || choice == '0') {
            if (write(fd, &choice, 1) < 0) {
                perror("Write failed");
            } else {
                printf("Sent '%c' to UART\n", choice);
            }
        } else {
            printf("Invalid choice\n");
        }
    }

    close(fd);
    return 0;
}
