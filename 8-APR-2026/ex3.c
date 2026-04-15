//    3. Append Text to File

// Write a program that appends user-provided text to an existing file using system calls.
// ***************************************************************************************
// Requirements

// Open file in append mode
// Read text from terminal
// Write text at the end of file
// Create file if it does not exist




#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>

void main(int argc, char **argv){

if(argc < 3){

printf("enter the correct formate form ./a.out SorceFileName AppendText\n");
return;
}


int fd = open(argv[1], O_APPEND|O_WRONLY, 0664);

if(fd < 0){
printf("%s  file is not present in the directory creating it........\n",argv[1]);
fd = open(argv[1], O_CREAT|O_TRUNC|O_RDWR, 0664);
}

write(fd, argv[2], strlen(argv[2]));

close(fd);
}







