#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
int main()
{
	int fd[2];
	int fd1[2];
        int pid;
	int a,b,result;
	if(pipe(fd)== -1 || pipe(fd1)== -1)
	{
		perror("pipe");
		return 1;
	}
	pid=fork();
	if(pid<0)
	{
		perror("fork");
		return 1;
	}
	if(pid>0)
	{
		close(fd[0]);
		//parent process
		printf("enter two numbes values:");
		scanf("%d%d",&a,&b);
		write(fd[1],&a,sizeof(int));
		write(fd[1],&b,sizeof(int));

		//
		close(fd1[1]);
		read(fd1[0],&result,sizeof(int));
		printf("%d",result);
				

		
	}
	else
	{
		//child
		close(fd[1]);
		read(fd[0],&a,sizeof(int));
		read(fd[0],&b,sizeof(int));
		result=a+b;
		//result send to the parent 
		close(fd1[0]);



		write(fd1[1],&result,sizeof(result));
	}
}
