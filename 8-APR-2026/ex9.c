/*

9. Execute Command with Input/Output Redirection

Write a program that runs ls and stores output in a file.

Requirements

Fork child
In child, redirect standard output to a file
Execute ls -l
Parent waits for completion

Practice focus
fork(), open(), dup2(), execvp(), wait()

*/



#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>




void main(){

int fd = open("abhi.txt", O_WRONLY|O_APPEND|O_CREAT, 0664);
if(fd < 0){
	perror("error while creating the file");
	return;
}

if(fork() == 0){
// in child
	if(dup2(fd, 1) < 0){ // 0-stdin, 1-stdout, 2-stderror
		
		perror("file not redirected");
		return;
	}

	printf(" hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh\n");
	fflush(stdout);	
	char *command [] = {"ls", "-l"};
	execvp(command[0], command);

	perror("child process terminated sussuccfuly\n");
}
else{
// in parent
waitpid(-1, 0, 0);
perror("parent procecess terminated successfully\n");
}


}






