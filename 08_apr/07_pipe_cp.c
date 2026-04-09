#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    int pipe_p2c[2];
    int pipe_c2p[2];
    pid_t pid;

    if (pipe(pipe_p2c) == -1 || pipe(pipe_c2p) == -1) {
        perror("pipe");
        return 1;
    }

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        close(pipe_p2c[1]);
        close(pipe_c2p[0]);

        int nums[2];
        read(pipe_p2c[0], &nums, sizeof(nums));
        close(pipe_p2c[0]);

        int sum = nums[0] + nums[1];
        write(pipe_c2p[1], &sum, sizeof(sum));
        close(pipe_c2p[1]);

        exit(0);
    } else {
        close(pipe_p2c[0]);
        close(pipe_c2p[1]);

        int nums[2] = {15, 27};
        printf("Parent sending %d and %d to child...\n", nums[0], nums[1]);
        write(pipe_p2c[1], &nums, sizeof(nums));
        close(pipe_p2c[1]);

        int result;
        read(pipe_c2p[0], &result, sizeof(result));
        close(pipe_c2p[0]);

        printf("Parent received result: %d\n", result);
        wait(NULL);
    }

    return 0;
}
