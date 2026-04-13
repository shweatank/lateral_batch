#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE *fp;
    char *device_path = "/dev/basic_char";
    char *expression = "50 990 -";
    char read_buffer[256];

    fp = fopen(device_path, "w");
    if (fp == NULL) {
        perror("Failed to open device for writing");
        return 1;
    }

    printf("Writing '%s' to %s...\n", expression, device_path);
    if (fprintf(fp, "%s", expression) < 0) {
        perror("Failed to write to device");
        fclose(fp);
        return 1;
    }
    
    fclose(fp);

    fp = fopen(device_path, "r");
    if (fp == NULL) {
        perror("Failed to open device");
        return 1;
    }

    printf("Reading result from %s...\n", device_path);

    if (fgets(read_buffer, sizeof(read_buffer), fp) != NULL) {
        printf("Driver Output: %s", read_buffer);
    } else {
        perror("Failed to read from device");
    }

    fclose(fp);
    return 0;
}
