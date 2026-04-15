#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>

void main(int argc, char**argv)
{


if(argc < 3){
printf("%d is invalid input--- provide ./a.out sourcefile destination file\n", argc);
return;
}

int fs = open(argv[1], O_RDONLY, 0664);

if(fs < 0){
printf("source file not present\n");
return;
}

int fd = open(argv[2], O_WRONLY| O_CREAT| O_TRUNC, 0664);

if(fd < 0)
{
printf("failed to open the destination file\n");
return;
}


char buffer[200];
int bytesread = 0;


while((bytesread = read (fs, buffer, sizeof(buffer))) > 0){

	write(fd, buffer, bytesread);
}

close(fd);
close(fs);

}
