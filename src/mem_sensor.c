#include "sysmon_ipc.h"

static volatile sig_atomic_t running = 1;

void handler(int sig) {
    running = 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [fd]\n", argv[0]);
        printf("This program reads data from memory sensor and writes it to a file descriptor.\n");
        return 0;
    }
    int fd = argc > 1 && atoi(argv[1]) != 0 ? atoi(argv[1]) : STDOUT_FILENO;
    
    FILE* fifo = fdopen(fd, "w");
    if(fifo == NULL){
        perror("Error opening FIFO");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting signal handler");
        return 1;
    }

    while (running) {
        FILE *file = fopen("/proc/meminfo", "r");
        if (file == NULL) {
            perror("Error opening /proc/meminfo");
            fclose(fifo);
            return 1;
        }

        MemData mem_data = {0};
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            if (strncmp(buffer, "MemTotal:", 9) == 0) {
                mem_data.mem_total = strtol(buffer + 9, NULL, 10);
            } else if (strncmp(buffer, "MemFree:", 8) == 0) {
                mem_data.mem_free = strtol(buffer + 8, NULL, 10);
            } else if (strncmp(buffer, "MemAvailable:", 13) == 0) {
                mem_data.mem_avail = strtol(buffer + 13, NULL, 10);
            } else if (strncmp(buffer, "SwapTotal:", 10) == 0) {
                mem_data.swap_total = strtol(buffer + 10, NULL, 10);
            } else if (strncmp(buffer, "SwapFree:", 9) == 0) {
                mem_data.swap_free = strtol(buffer + 9, NULL, 10);
            }
        }

        fclose(file);
        if(fwrite(&mem_data, sizeof(MemData), 1, fifo) != 1) {
            perror("Error writing memory data");
            fclose(fifo);
            return 1;
        }
        fflush(fifo);

        sleep(INTERVAL);
    }
    
    return 0;
}
