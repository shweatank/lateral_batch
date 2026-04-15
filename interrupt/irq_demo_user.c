#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;

    fd = open("/dev/irq_demo", O_WRONLY); // Open device

    int data[3];

    data[0] = 10; // First value
    data[1] = 5;  // Second value
    data[2] = 1;  // Operation (1=add,2=sub,3=mul,4=div)

    write(fd, data, sizeof(data)); // Send to kernel

    close(fd); // Close device

    return 0;
}
