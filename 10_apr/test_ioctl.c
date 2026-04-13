#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define IOCTL_MAGIC 'B'
#define IOCTL_SET_VALUE _IOWR(IOCTL_MAGIC, 1, int)

int main() {
    int fd;
    int value = 42;

    fd = open("/dev/basic_ioctl", O_RDWR);
    if (fd < 0) {
        perror("Failed to open");
        return 1;
    }

    printf("USER: Sending value %d to kernel\n", value);

    ioctl(fd, IOCTL_SET_VALUE, &value);

    printf("USER: got back %d from kernel \n", value);

    close(fd);
    return 0;
}
