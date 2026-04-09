#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

int main() {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        char prompt[] = "minishell> ";
        write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);

        int bytes_read = read(STDIN_FILENO, line, sizeof(line) - 1);
        if (bytes_read <= 0) {
            break;
        }
        
        line[bytes_read] = '\0';

        if (line[bytes_read - 1] == '\n') {
            line[bytes_read - 1] = '\0';
        }

        if (strcmp(line, "exit") == 0) {
            break;
        }

        if (strlen(line) == 0) {
            continue;
        }

        int i = 0;
        args[i] = strtok(line, " \t");
        while (args[i] != NULL && i < MAX_ARGS - 1) {
            i++;
            args[i] = strtok(NULL, " \t");
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
        } else if (pid == 0) {
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else {
            wait(NULL);
        }
    }

    return 0;
}
