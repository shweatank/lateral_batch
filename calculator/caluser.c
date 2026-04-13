#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main()
{
    int fd;
    char *text = "2 3 +";
    char buffer[100];
    ssize_t bytes;

    fd = open("/dev/calchar", O_RDWR);
    if (fd < 0)
    {
        perror("Open failed");
        return 1;
    }

    write(fd, text, strlen(text));
    lseek(fd, 0, SEEK_SET);

    bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes < 0)
    {
        perror("Read failed");
        close(fd);
        return 1;
    }

    buffer[bytes] = '\0';

    printf("Output from driver:\n%s", buffer);

    close(fd);
    return 0;
}
