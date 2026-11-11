#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [interval]\n", argv[0]);
        printf("interval: Time in seconds between updates (default is 1 second)\n");
        return 0;
    }
    
    useconds_t interval = 1000000;
    if(argc > 1){
        interval = atof(argv[1]) * 1000000;
    }

    while(1){
        size_t total = 0;

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

        struct dirent *entry;
        while((entry = readdir(dir)) != NULL) {
            if (atoi(entry->d_name) > 0) {
                total++;
            }
        }
        
        fprintf(file_tmp, "Total number of processes: %zu\n", total);
        fclose(file_tmp);
        closedir(dir);
        usleep(interval);
    }

    return 0;
}