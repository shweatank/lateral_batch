#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
int main()
{
	int fd[2];
	pid_t pid;
	char buf[1024];
	if(pipe(fd)==-1)
	{
		perror("pipe");
		return 1;
	}
	pid=fork();
	if(pid<0)
	{
		perror("fork error");
		return 1;
	}
	char str[]="hello wrold";
	if(pid>0)
	{
		// parent process
		close(fd[0]);
		write(fd[1],str,strlen(str)+1);
		close(fd[1]);
	}
	else
	{
		close(fd[1]);
		read(fd[0],buf,1024);
		printf("%s",buf);
		close(fd[0]);
	}
}

		


