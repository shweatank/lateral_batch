#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        int fd = open("ls_output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }

        dup2(fd, STDOUT_FILENO);
        close(fd);

        char *args[] = {"ls", "-l", NULL};
        execvp("ls", args);
        perror("execvp");
        exit(1);
    } else {
        wait(NULL);
        printf("Command executed, check ls_output.txt\n");
    }

    return 0;
}
