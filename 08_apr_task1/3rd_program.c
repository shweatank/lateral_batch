#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#define BUFFER_SIZE 1024
int main(int argc, char *argv[])
{
	int fd;
	char buffer[BUFFER_SIZE];
	if(argc!=2)
	{
		printf("invalid arguments");
		return 1;
	}
	int bytes_read;
	fd=open(argv[1],O_WRONLY|O_CREAT|O_APPEND,0644);
	if(fd<0)
	{
		perror("error opening file");
		return 1;
	}
	while((bytes_read=read(0,buffer,BUFFER_SIZE))>0)
	{
		write(fd,buffer,bytes_read);
	}
	
		printf("successfully written into the file");
	close(fd);
}
