#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "sysmon_ipc.h"

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from process sensor and displays it.\n");
        return 0;
    }
    
    mkdir("/tmp/sysmon", 0777);

    while(1){
        DIR *dir = opendir("/proc");
        if (dir == NULL) {
            perror("Error opening /proc");
            return 1;
        }
    
        FILE *file_tmp = fopen("/tmp/sysmon/proc.dat", "w");
        if(file_tmp == NULL) {
            perror("Error opening /tmp/sysmon/proc.dat");
            closedir(dir);
            return 1;
        }

        size_t total = 0;
        struct dirent *entry;

        while((entry = readdir(dir)) != NULL) {
            if (atoi(entry->d_name) > 0) {
                total++;
            }
        }
        
        fprintf(file_tmp, "Total number of processes: %zu\n", total);

        fclose(file_tmp);
        closedir(dir);

        sleep(INTERVAL);
    }
    
    return 0;
}