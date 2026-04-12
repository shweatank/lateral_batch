#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
	int src_fd,dest_fd;
        char buffer[BUFFER_SIZE];
	ssize_t bytes_read, bytes_written;
	if(argc!=3)
	{
		printf("usage:%s <source> <destination>\n",argv[0]);
		return 1;
	}
	src_fd=open(argv[1],O_RDONLY);
	if(src_fd < 0)
	{
		perror("Error opening source file");
		return 1;
	}
	//open/create destination file
	dest_fd=open(argv[2],O_WRONLY|O_CREAT|O_TRUNC,0644);
	if(dest_fd < 0)
	{
		perror("Error opening destination file");
		close(src_fd);
		return 1;
	}
	while((bytes_read=read(src_fd,buffer,BUFFER_SIZE))>0)
	{
		bytes_written=write(dest_fd,buffer,bytes_read);
		if(bytes_written!=bytes_read)
		{
		perror("write error");
		close(src_fd);
		close(dest_fd);
		return 1;
		}
	}
	if(bytes_read < 0 )
		perror("Read error");
	close(src_fd);
	close(dest_fd);
	printf("File copied successfully.\n");
	return 0;
}


