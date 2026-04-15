/* 7. Child to Parent Communication

Write a program where child sends calculated result back to parent through a pipe.

Requirements

Parent sends two integers
Child reads them, adds them
Child sends result back
Parent prints final result

Practice focus
pipe(), fork(), read(), write()

*/



#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>


/*

void main(){


int fd[2];

if(pipe(fd) < 0){
	printf("failed to creat the pipe\n");
	return;
}

	if(fork() == 0){
	
	int arr[2];

	read(fd[0], arr, sizeof(arr));
	close(fd[0]);

	int sum = arr[0] + arr[1];
	
	write(fd[1], &sum, sizeof(sum));
	close(fd[1]);

	}
	else{
	
	int arr[2] = {10, 20};

	write(fd[1], arr, sizeof(arr));
	close(fd[1]);
	
	waitpid(-1, 0, 0);	
	
	int result;

	read(fd[0], &result, sizeof(result));
	close(fd[0]);
	
	printf("the result is %d\n", result);
	}

}

*/



void main(){

int fd1[2];
int fd2[2];

if((pipe(fd1)<0) | (pipe(fd2) < 0)){
	printf("pipe is not created\n");
	return;
}

if(fork() == 0){

	int num1, num2, sum;

	close(fd2[1]);

	read(fd1[0], &num1, sizeof(num1));
	read(fd2[0], &num2, sizeof(num2));

	sum = num1 + num2;

	write(fd1[1], &sum, sizeof(sum));

	close(fd1[0]);
	close(fd2[0]);
	close(fd1[1]);
	close(fd2[1]);




}
else{

	int num1 = 10, num2 = 30, result = 0;
	
	write(fd1[1], &num1, sizeof(num1));
	write(fd2[1], &num2, sizeof(num2));

	waitpid(-1, 0, 0);

	read(fd1[0], &result, sizeof(result));
	
	printf("the result is : %d\n", result);

	close(fd1[0]);
	close(fd2[0]);
	close(fd1[1]);
	close(fd2[1]);
}

}










