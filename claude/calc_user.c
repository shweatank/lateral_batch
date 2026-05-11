#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "calc_ioctl.h"

#define DEVICE_PATH "/dev/calc"

int main(int argc, char *argv[])
{
    struct calc_data data;
    int fd;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <num1> <num2> <add|sub>\n", argv[0]);
        return 1;
    }

    data.num1 = atoi(argv[1]);
    data.num2 = atoi(argv[2]);

    if (strcmp(argv[3], "add") == 0)
        data.op = CALC_OP_ADD;
    else if (strcmp(argv[3], "sub") == 0)
        data.op = CALC_OP_SUB;
    else {
        fprintf(stderr, "operation must be 'add' or 'sub'\n");
        return 1;
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open " DEVICE_PATH);
        return 1;
    }

    if (ioctl(fd, CALC_COMPUTE, &data) < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("%d %s %d = %d\n",
           data.num1,
           data.op == CALC_OP_ADD ? "+" : "-",
           data.num2,
           data.result);

    close(fd);
    return 0;
}
