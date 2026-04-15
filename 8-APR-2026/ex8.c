/*
8. Redirect Output to File

Write a program that redirects standard output to a file and then prints text.

Requirements

Open output file
Redirect stdout to that file
Print some text using low-level output
Verify output goes into file

Practice focus
open(), dup(), dup2(), write(), close()

*/


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

void main(){

	int fd;


	fd = open("abhi.txt", O_WRONLY | O_CREAT| O_APPEND, 0664);

	if(fd < 0)
	{
		printf("error tomopen the file\n");
		return;
	}


	if(dup2(fd, 1) < 0){    // 0 - stdin, 1 - stdout, 2 - stderror
		
		printf("duplication of file is failed:\n");
		return;
	}
	
	char p[] = "tamopahaa technologies 1234567890";
	write(1, p, sizeof(p));

	close(fd);
	

}
