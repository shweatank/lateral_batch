
/*
10. Simple Shell

Write a mini shell using system calls.

Requirements

Show prompt
Read command from user
Parse command into program and arguments
Fork and execute command
Wait for child
Exit on typing exit

Practice focus
read(), write(), fork(), execvp(), wait()

*/


#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<string.h>


#define MAXINPUT_PROMT	500
#define MAXCOMMAND	30
void main(void){


char input[MAXINPUT_PROMT];
char *argv[MAXCOMMND];


void parse_command(char *s, char **p)
{

for (int i = 0; i <= (strlen(s)); i++)
{

}

}

char promt[] = "enter the command to execute"; 

while()
{
	write(1, , promt, sizeof(prompt));
	int n = read(0, input, sizeof(input) - 1)
	if(n <= 0)
		continue;
	input[n] = '\0';

	if(strcmp(input, "exit") == 0)
		break;

	parse_command(input, argv);


}



}
