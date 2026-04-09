#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>

int main() {
    int fd = open("file.txt", O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        return 1;
    }

    char buf[1024];
    int bytes_read;
    int lines = 0, words = 0, chars = 0;
    int in_word = 0;

    while ((bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < bytes_read; i++) {
            chars++;
            if (buf[i] == '\n') {
                lines++;
            }
            if (isspace(buf[i]) || buf[i] == '\0') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
    }

    if (bytes_read < 0) {
        perror("Error reading file");
        close(fd);
        return 1;
    }

    close(fd);

    printf("Lines: %d\nWords: %d\nCharacters: %d\n", lines, words, chars);
    return 0;
}
