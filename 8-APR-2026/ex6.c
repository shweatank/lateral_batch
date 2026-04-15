/*


6. Parent to Child Communication

Write a program where parent sends a message to child using a pipe.

Requirements

Create a pipe
Fork a child
Parent writes message into pipe
Child reads and prints it
Close unused pipe ends properly

Practice focus
pipe(), fork(), read(), write(), close()

*/


#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>


void main(){

	int pipes[2];

	if(pipe(pipes) < 0){   // creating a pipe to use as file operation
				// we need to use 0 for read 
				// 	use 1 for write  it is default we cant chage this property

		printf("failed to creat the pipe\n");
		return;
	}

	if(fork() == 0){
	
	char buffer1[10];

	close(pipes[1]);

	read(pipes[0], buffer1, sizeof(buffer1));
	printf("recieved fron the pearent %s\n", buffer1);

	}
	else{
		close(pipes[0]);

		char buffer[] = "abishek";
		write(pipes[1], buffer, sizeof(buffer));
	}
}

