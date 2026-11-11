#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [interval]\n", argv[0]);
        printf("interval: Time in seconds between updates (default is 1 second)\n");
        return 0;
    }

    useconds_t interval = 1;
    if(argc > 1){
        interval = atof(argv[1]) * 1000000;
    }

    mkdir("/tmp/sysmon", 0777);

    while(1) {
        FILE *file = fopen("/proc/meminfo", "r");
        if(file == NULL) {
            perror("Error opening /proc/meminfo");
            return 1;
        }
        
        FILE *file_tmp = fopen("/tmp/sysmon/mem.dat", "w");
        if(file_tmp == NULL) {
            perror("Error opening /tmp/sysmon/mem.dat");
            fclose(file);
            return 1;
        }

        char buffer[256];
        while(fgets(buffer, sizeof(buffer), file) != NULL) {
            if(strncmp(buffer, "MemTotal:",      9) == 0
            || strncmp(buffer, "MemFree:",       8) == 0
            || strncmp(buffer, "MemAvailable:", 13) == 0
            || strncmp(buffer, "SwapTotal:",    10) == 0
            || strncmp(buffer, "SwapFree:",      9) == 0) {
                fprintf(file_tmp, "%s", buffer);
            }
        }

        fclose(file);
        fclose(file_tmp);
        usleep(interval); // Update every second
    }
    return 0;
}