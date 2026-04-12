#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#define BUFFER_SIZE 1024
int main(int argc, char *argv[])
{
	char buffer[BUFFER_SIZE];
	int fd;
	if(argc!=2)
	{
		printf("invalid arguments");
		return 1;
	}
	ssize_t bytes_read;
	fd=open(argv[1],O_RDONLY);
	if(fd<0)
	{
		perror("open");
		return 1;
	}
	int i,char_count=0,line_count=0;
	while((bytes_read=read(fd,buffer,BUFFER_SIZE))>0)
	{
		for(i=0;i<bytes_read;i++)
		{
			char_count++;
			if(buffer[i]=='\n')
				line_count++;
		}
	}
	printf("character_count=%d",char_count);
	printf("line_count=%d",line_count);
}
		
