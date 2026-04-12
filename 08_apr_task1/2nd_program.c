#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#define BUFFER_SIZE 1024
int main(int argc,char *argv[])
{
	int read_fd;
	if(argc!=2)
	{
		printf("given arguments are wronge");
		return 1;
	}
	read_fd=open(argv[1], O_RDONLY);
	if(read_fd<0)
	{
		perror("error opening readfile");
		return 1;
	}

	ssize_t bytes_read;
	char buffer[BUFFER_SIZE];
	while((bytes_read=read(read_fd,buffer,BUFFER_SIZE))>0)
	{
		write(1,buffer,bytes_read);
	}
	close(read_fd);
}



