#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>

// cat command practive using the system call

void main(int argc, char **argv){

if(argc < 2){
printf("provide the proper formate\n" );
return;
}


int fd;

for(int i = 1; i < argc ; i++ )
{

	fd = open(argv[i], O_RDONLY);
	
	if(fd < 0)
	{
		printf("\n %s : no such files\n -----------------------------------------\n", argv[i] );
		continue;
	}

	char buffer[1024]="\0";
	int bytesread = 0;
	while((bytesread = read(fd, buffer, sizeof(buffer))) > 0)
		printf("%s",buffer);
	
	close(fd);
}



}
