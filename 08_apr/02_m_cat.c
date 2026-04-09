#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd, n;
    char buffer[1024];

    fd = open("file.txt", O_RDONLY);
    if(fd < 0)
        return 1;

    while((n = read(fd, buffer, sizeof(buffer))) > 0)
        write(1, buffer, n);

    close(fd);

    return 0;
}