#include "sysmon_ipc.h"

volatile int fd;

void handler(int sig) {
    close(fd);
    unlink(CPU_FIFO_PATH);
    exit(0);
}

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from CPU sensor and displays it.\n");
        return 0;
    }

    int interval;
    if(argc > 1 && atoi(argv[1]) != 0) {
        interval = atoi(argv[1]);
    } else {
        interval = INTERVAL;
    }

    if(mkfifo(CPU_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating CPU_FIFO_PATH");
        return 1;
    }

    fd = open(CPU_FIFO_PATH, O_WRONLY);
    if(fd == -1) {
        perror("Error opening CPU_FIFO_PATH");
        return 1;
    }
    
    struct sigaction sa;
    sa.sa_handler = &handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Error setting signal handler");
        return 1;
    }
    
    while(1) {
        FILE *file = fopen("/proc/loadavg", "r");
        if(file == NULL) {
            perror("Error opening /proc/loadavg");
            close(fd);
            return 1;
        }
        double loadavg1, loadavg5, loadavg15;
        CpuData  cpu_data;

        if(fscanf(file, "%lf %lf %lf", &loadavg1, &loadavg5, &loadavg15) == 3) {
            cpu_data.load1 = loadavg1;
            cpu_data.load5 = loadavg5;
            cpu_data.load15 = loadavg15;
        } else {
            write(fd, "Error reading load averages\n", strlen("Error reading load averages\n"));
        }

        if(write(fd, &cpu_data, sizeof(CpuData)) == -1) {
            perror("Error writing to CPU_FIFO_PATH");
            return 1;
        }
        
        fclose(file);
        sleep(interval);
    }

    return 0;
}