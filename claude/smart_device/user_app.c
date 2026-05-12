#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>

#include "smart_ioctl.h"

#define DEVICE_PATH "/dev/smart_device"
#define MAX_BUFFER 1024

void print_menu() {
    printf("\n");
    printf("===========================================\n");
    printf("  SMART DEVICE DRIVER - USER APPLICATION   \n");
    printf("===========================================\n");
    printf("1. Write Data to Device\n");
    printf("2. Read Data from Device\n");
    printf("3. [IOCTL] Get Driver Statistics\n");
    printf("4. [IOCTL] Clear Internal Buffer\n");
    printf("5. [IOCTL] Get Device Mode\n");
    printf("6. [IOCTL] Set Device Mode\n");
    printf("7. [IOCTL] Get Last Command\n");
    printf("8. [IOCTL] Toggle Logging (Enable/Disable)\n");
    printf("9. [IOCTL] Reset Device\n");
    printf("10. Automated Stress Test\n");
    printf("0. Exit\n");
    printf("===========================================\n");
    printf("Enter choice: ");
}

/* Test routine for multithreading stress test */
void* stress_thread(void* arg) {
    int fd = *(int*)arg;
    for (int i = 0; i < 50; i++) {
        char msg[64];
        sprintf(msg, "Stress test message %d from thread %ld", i, pthread_self());
        if (write(fd, msg, strlen(msg)) < 0) {
            perror("Stress test write failed");
        }
        
        int mode = MODE_NORMAL;
        ioctl(fd, SET_DEVICE_MODE, &mode);
    }
    return NULL;
}

int main() {
    int fd, choice;
    char buffer[MAX_BUFFER];
    struct smart_stats stats;
    int tmp_val;
    char last_cmd[256];
    int logging_state = 1;

    /* Open the device node */
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        printf("Ensure the driver is loaded and /dev/smart_device exists with proper permissions (sudo).\n");
        return EXIT_FAILURE;
    }

    printf("Device %s opened successfully.\n", DEVICE_PATH);

    while (1) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exiting.\n");
            break;
        }
        
        /* Clear stdin buffer */
        while(getchar() != '\n'); 

        switch (choice) {
            case 1:
                printf("Enter string to write: ");
                if (fgets(buffer, MAX_BUFFER, stdin) == NULL) {
                    printf("Error reading input.\n");
                    continue;
                }
                buffer[strcspn(buffer, "\n")] = 0; /* Remove newline */
                
                ssize_t written = write(fd, buffer, strlen(buffer));
                if (written < 0) {
                    perror("Failed to write to the device");
                } else {
                    printf("Successfully wrote %zd bytes.\n", written);
                }
                break;

            case 2:
                memset(buffer, 0, MAX_BUFFER);
                /* Seek to start not explicitly required if we rely on driver semantics, 
                   but let's read directly since our read handles offset locally per fd. */
                ssize_t bytes_read = read(fd, buffer, MAX_BUFFER - 1);
                if (bytes_read < 0) {
                    perror("Failed to read from the device");
                } else if (bytes_read == 0) {
                    printf("EOF reached (no more data).\n");
                } else {
                    printf("Read %zd bytes: %s\n", bytes_read, buffer);
                }
                break;

            case 3:
                if (ioctl(fd, GET_DRIVER_STATS, &stats) < 0) {
                    perror("Failed to get driver stats");
                } else {
                    printf("\n--- DRIVER STATISTICS ---\n");
                    printf("Open Count:     %lu\n", stats.open_count);
                    printf("IOCTL Count:    %lu\n", stats.ioctl_count);
                    printf("Bytes Read:     %lu\n", stats.bytes_read);
                    printf("Bytes Written:  %lu\n", stats.bytes_written);
                    printf("-------------------------\n");
                }
                break;

            case 4:
                if (ioctl(fd, CLEAR_BUFFER) < 0) {
                    perror("Failed to clear buffer");
                } else {
                    printf("Device buffer cleared successfully.\n");
                }
                break;

            case 5:
                if (ioctl(fd, GET_DEVICE_MODE, &tmp_val) < 0) {
                    perror("Failed to get device mode");
                } else {
                    printf("Current Device Mode: %d\n", tmp_val);
                    if (tmp_val == MODE_NORMAL) printf("(MODE_NORMAL)\n");
                    else if (tmp_val == MODE_STRICT) printf("(MODE_STRICT)\n");
                    else if (tmp_val == MODE_LOOPBACK) printf("(MODE_LOOPBACK)\n");
                }
                break;

            case 6:
                printf("Enter mode (0=Normal, 1=Strict, 2=Loopback): ");
                if (scanf("%d", &tmp_val) != 1) {
                    printf("Invalid input.\n");
                    continue;
                }
                if (ioctl(fd, SET_DEVICE_MODE, &tmp_val) < 0) {
                    perror("Failed to set device mode");
                } else {
                    printf("Device mode set to %d.\n", tmp_val);
                }
                break;

            case 7:
                if (ioctl(fd, GET_LAST_COMMAND, last_cmd) < 0) {
                    perror("Failed to get last command");
                } else {
                    printf("Last command/string written: '%s'\n", last_cmd);
                }
                break;

            case 8:
                if (logging_state) {
                    if (ioctl(fd, DISABLE_LOGGING) == 0) {
                        printf("Logging DISABLED.\n");
                        logging_state = 0;
                    }
                } else {
                    if (ioctl(fd, ENABLE_LOGGING) == 0) {
                        printf("Logging ENABLED.\n");
                        logging_state = 1;
                    }
                }
                break;

            case 9:
                if (ioctl(fd, RESET_DEVICE) < 0) {
                    perror("Failed to reset device");
                } else {
                    printf("Device state reset to defaults.\n");
                }
                break;

            case 10:
                printf("Starting Stress Test (Concurrency)...\n");
                pthread_t t1, t2, t3;
                pthread_create(&t1, NULL, stress_thread, &fd);
                pthread_create(&t2, NULL, stress_thread, &fd);
                pthread_create(&t3, NULL, stress_thread, &fd);
                
                pthread_join(t1, NULL);
                pthread_join(t2, NULL);
                pthread_join(t3, NULL);
                printf("Stress test complete. Check dmesg and GET_DRIVER_STATS.\n");
                break;

            case 0:
                printf("Exiting application...\n");
                close(fd);
                return EXIT_SUCCESS;

            default:
                printf("Invalid choice. Try again.\n");
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}
