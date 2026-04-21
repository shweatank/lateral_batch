#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd, n;
    char buf[100];
    char *data = "50 10 +";

    fd = open("/proc/proc_demo", O_RDWR);
    if (fd < 0) {
        perror("open failed");
        return 1;
    }
    write(fd, data, 8);
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read failed");
        return 1;
    }
    buf[n] = '\0';

    printf("Result: %s", buf);

    close(fd);
    return 0;
}