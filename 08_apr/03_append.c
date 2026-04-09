#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("file.txt", O_CREAT | O_WRONLY | O_APPEND, 0644);
    if(fd < 0)
        return 1;

    write(fd, "hello mirafra\n", 15);

    close(fd);

    return 0;
}