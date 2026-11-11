#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [interval]\n", argv[0]);
        printf("interval: Time in seconds between updates (default is 1 second)\n");
        printf("This program reads data from CPU sensor and displays it.\n");
        return 0;
    }

    useconds_t interval = 1000000;
    if(argc > 1){
        interval = atof(argv[1]) * 1000000;
    }
    
    while(1) {
        double loadavg1, loadavg5, loadavg15;
    
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

        if(fscanf(file, "%lf %lf %lf", &loadavg1, &loadavg5, &loadavg15) == 3) {
            fprintf(file_tmp, "Load Average (1 min): %.2f\n", loadavg1);
            fprintf(file_tmp, "Load Average (5 min): %.2f\n", loadavg5);
            fprintf(file_tmp, "Load Average (15 min): %.2f\n", loadavg15);
        } else {
            fprintf(file_tmp, "Error reading load averages\n");
        }

        fclose(file);
        fclose(file_tmp);

        usleep(interval);
    }

    return 0;
}