#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_NAME "/dev/ioctl_cal"
#define IOCTL_MAGIC 'C'

struct calc_data {
    int a;
    int b;
    char op;
    int result;
};

#define IOCTL_CALC _IOWR(IOCTL_MAGIC, 1, struct calc_data)

int main()
{
    int fd;
    struct calc_data data;

    fd = open(DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("Enter first number: ");
    scanf("%d", &data.a);

    printf("Enter operation (+, -, *, /): ");
    scanf(" %c", &data.op);

    printf("Enter second number: ");
    scanf("%d", &data.b);

    if (ioctl(fd, IOCTL_CALC, &data) < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("Result = %d\n", data.result);

    close(fd);
    return 0;
}
