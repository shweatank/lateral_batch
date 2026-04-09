#include <fcntl.h>
#include <unistd.h>

int main()
{
    int src, dest, n;
    char buffer[1024];

    src = open("file.txt", O_RDONLY);
    if(src < 0)
        return 1;

    dest = open("file1.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);

    while((n = read(src, buffer, sizeof(buffer))) > 0)
        write(dest, buffer, n);

    close(src);
    close(dest);

    return 0;
}