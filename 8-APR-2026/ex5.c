/*	
	5. Implement Basic system() Behavior
	
	Write a program that runs another command using process creation system calls.
	
	Requirements
	
	Create a child process
	Child should execute a command like ls -l
	Parent should wait for child completion
	Print child exit status

	Practice focus
	fork(), execvp(), wait() / waitpid()

*/


#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>


void main(){

if(fork() == 0){
// child process
char *command[] = {"ls","-l", 0};

execvp(command[0],command);
perror("execvp failed\n");

}
else{
// parent process

int s;
waitpid(-1, &s, 0); // waiting any child process to complete, not interest to collect status, 

if(WIFEXITED(s)){
printf("Child exited sussuccfully\n");
}
else
printf("child exites abnormally\n");
}


}



