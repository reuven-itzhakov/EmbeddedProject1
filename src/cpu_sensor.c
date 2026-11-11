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

    while(1) {
        FILE *file = fopen("/proc/loadavg", "r");
        if(file == NULL) {
            perror("Error opening /proc/loadavg");
            return 1;
        }
        
        FILE *file_tmp = fopen("/tmp/sysmon/cpu.dat", "w");
        if(file_tmp == NULL) {
            perror("Error opening /tmp/sysmon/cpu.dat");
            fclose(file);
            return 1;
        }
        
        char buffer[256];
        while(fgets(buffer, sizeof(buffer), file) != NULL) {
            fprintf(file_tmp, "%s", buffer);
        }

        fclose(file);
        fclose(file_tmp);
        usleep(interval); // Update every second
    }
    return 0;
}