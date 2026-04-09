#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main() {
    int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    int old_stdout = dup(STDOUT_FILENO);
    
    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("dup2");
        return 1;
    }
    
    close(fd);

    char text[] = "This text will go into the file instead of the terminal.\n";
    write(STDOUT_FILENO, text, strlen(text));
    printf("Even printf statements go to the file!\n");

    fflush(stdout); 
    dup2(old_stdout, STDOUT_FILENO);
    close(old_stdout);

    printf("Stdout restored, verification complete.\n");

    return 0;
}
