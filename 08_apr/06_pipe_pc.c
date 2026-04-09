#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

int main() {
    int pipefd[2];
    pid_t pid;
    char msg[] = "Hello from Parent!";
    char buf[1024];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        close(pipefd[1]);
        int bytes_read = read(pipefd[0], buf, sizeof(buf) - 1);
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            printf("Child received: %s\n", buf);
        }
        close(pipefd[0]);
        exit(0);
    } else {
        close(pipefd[0]);
        write(pipefd[1], msg, strlen(msg));
        close(pipefd[1]);
        wait(NULL);
    }

    return 0;
}
