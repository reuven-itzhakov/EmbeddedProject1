#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "sysmon_ipc.h"
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from CPU sensor and displays it.\n");
        return 0;
    }

    // /tmp/sysmon_cpu.fifo
    
    while(1) {
        double loadavg1, loadavg5, loadavg15;
        CpuData  cpu_data;

        int fd = open("/tmp/sysmon_cpu.fifo", O_WRONLY);
        if(fd == -1) {
            perror("Error opening /tmp/sysmon_cpu.fifo");
            return 1;
        }

        FILE *file = fopen("/proc/loadavg", "r");
        if(file == NULL) {
            perror("Error opening /proc/loadavg");
            close(fd);
            return 1;
        }
        
        if(fscanf(file, "%lf %lf %lf", &loadavg1, &loadavg5, &loadavg15) == 3) {
            cpu_data.load1 = loadavg1;
            cpu_data.load5 = loadavg5;
            cpu_data.load15 = loadavg15;
        } else {
            write(fd, "Error reading load averages\n", strlen("Error reading load averages\n"));
        }

        if(write(fd, &cpu_data, sizeof(CpuData)) == -1) {
            perror("Error writing to /tmp/sysmon_cpu.fifo");
        }

        fclose(file);
        close(fd);

        sleep(INTERVAL);
    }

    return 0;
}