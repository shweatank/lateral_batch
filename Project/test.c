#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
int main() {
    int lcd_fd;

    lcd_fd = open("/dev/ili9225", O_WRONLY);
    if (lcd_fd < 0) {
        perror("open lcd");
        return 1;
    }

    printf("LCD opened successfully\n");

    while(1) sleep(1);
}
