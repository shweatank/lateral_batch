/*

	4. Count Lines, Words, and Characters

	Write a program that reads a file and counts:
	
	total lines
	total words
	total characters

	Requirements

	// Use system calls only for file access
	// Do not use high-level C library file APIs
	// Handle multiple spaces and newlines correctly

	// Practice focus
	// open(), read(), close()


*/



int fileCountLine(int fd);
int fileCountCharacter(int fd);
int fileCountWord(int fd);
void movetovalidposition(int fd);





#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>




void main(int argc, char **argv){

if (argc != 2){
printf("invalid input.......:Syntax ./a.out FileName\n");
return;
}


int fd = open(argv[1], O_RDONLY,0664);

if(fd < 0){
printf("invalid file Name: %s \n",argv[1]);
return;
}

int linecount = fileCountLine(fd);
printf("number of lines in the %s file : %d\n", argv[1], linecount);

int charactercount = fileCountCharacter(fd);
printf("number of character in the %s file : %d \n", argv[1], charactercount );

int wordcount = fileCountWord(fd);
printf("number word in %s : %d \n", argv[1], wordcount);
}



int fileCountLine(int fd){

int bytesread = 0;

char ch;
int lineCount = 0;

lseek(fd, 0, SEEK_SET); // bring the file descriptor to initial of the file

while((bytesread = read(fd,&ch, 1)) > 0){

	if(ch == '\n')
		lineCount++;
}

return lineCount;
}


int fileCountCharacter(int fd){

int bytesread = 0;
char ch;
int charcount = 0;

lseek(fd, 0, SEEK_SET); // bringing file descriptpr to begining of the file

while((bytesread = read(fd,&ch, 1 )) > 0){

	// if(ch == '\n')
		charcount++;
}

return charcount;

}


int fileCountWord(int fd){

int bytesread = 0 ;
int wordcount = 0;
char ch; 

lseek(fd, 0, SEEK_SET);

while((bytesread = read(fd, &ch, 1)) > 0){
	
	if(ch == ' '| ch == '\n'){
		wordcount++;
	movetovalidposition(fd);
	}

}

return wordcount;
}



void movetovalidposition(int fd){

int byteread = 0 ;
char ch;

while((byteread = read(fd, &ch, 1)) > 0){
	
	if(ch == ' ' | ch == '\n')
		continue;
	
	break;
}
}
