#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sysmon_ipc.h"

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from memory sensor and displays it.\n");
        return 0;
    }
    
    MemData mem_data;

    while(1) {

        FILE* fifo;
        fifo = fdopen(argc > 1 && atoi(argv[1]) != 0 ? atoi(argv[1]) : STDOUT_FILENO, "w");
        if(fifo == NULL){
            perror("Error opening FIFO");
            return 1;
        }

        FILE *file = fopen("/proc/meminfo", "r");
        if(file == NULL) {
            perror("Error opening /proc/meminfo");
            fclose(fifo);
            return 1;
        }
        
        char buffer[256];
        while(fgets(buffer, sizeof(buffer), file) != NULL) {
            if(strncmp(buffer, "MemTotal:", 9) == 0){
                mem_data.mem_total = atol(buffer + 9);
            }
            if(strncmp(buffer, "MemFree:", 8) == 0){
                mem_data.mem_free = atol(buffer + 8);
            }
            if(strncmp(buffer, "MemAvailable:", 13) == 0){
                mem_data.mem_avail = atol(buffer + 13);
            }
            if(strncmp(buffer, "SwapTotal:", 10) == 0){
                mem_data.swap_total = atol(buffer + 10);
            }
            if(strncmp(buffer, "SwapFree:", 9) == 0) {
                mem_data.swap_free = atol(buffer + 9);
            }

        }

        fclose(file);
        fclose(fifo);

        sleep(INTERVAL);
    }
    
    return 0;
}