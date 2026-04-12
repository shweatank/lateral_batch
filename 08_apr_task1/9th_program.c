#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/wait.h>
int main()
{
	pid_t pid;


	pid=fork();
        if(pid<0)
	{
		perror("fork failed");
		exit(1);
	}
	//child process
	if(pid == 0)
	{
		int fd;
		//open file for writting
		fd=open("output_file.txt", O_WRONLY|O_CREAT|O_TRUNC,0x664);
		if(fd<0)
		{
			perror("faile open failed");
			exit(1);
		}
		//redirect stdout to file
		dup2(fd,1);
		close(fd);
		char *args[]={"ls","-l",NULL};
		execvp("ls",args);
		perror("excv failed");
		exit(1);
	}
	//parent process
	else
	{
		wait(NULL);
		printf("child completed output stored in output.txt\n");
	}
	return 0;
}




