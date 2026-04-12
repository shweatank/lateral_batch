#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
//file descriptor + redirection problem
int main()
{
	int fd;
	fd=open("output.txt",O_WRONLY|O_CREAT|O_TRUNC,0x664);
	if(fd==-1)
	{
		perror("open");
		return 1;
	}
	//redirect file to the stdout
	dup2(fd,1);
	write(1,"hello bhavani",13);
	close(fd);
}

