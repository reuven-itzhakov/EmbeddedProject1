#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [interval]\n", argv[0]);
        printf("interval: Time in seconds between updates (default is 1 second)\n");
        printf("This program reads data from memory, CPU, and process sensors and displays it.\n");
        return 0;
    }

    useconds_t interval = 1000000;
    if(argc > 1){
        interval = atof(argv[1]) * 1000000;
    }

    mkdir("/tmp/sysmon", 0777);
    
    while(1) {
        system("clear");

        FILE *mem_file = fopen("/tmp/sysmon/mem.dat", "r");
        if(mem_file == NULL) {
            perror("Error opening /tmp/sysmon/mem.dat");
            return 1;
        }
        
        FILE *cpu_file = fopen("/tmp/sysmon/cpu.dat", "r");
        if(cpu_file == NULL) {
            perror("Error opening /tmp/sysmon/cpu.dat");
            fclose(mem_file);
            return 1;
        }
        
        FILE *proc_file = fopen("/tmp/sysmon/proc.dat", "r");
        if(proc_file == NULL) {
            perror("Error opening /tmp/sysmon/proc.dat");
            fclose(mem_file);
            fclose(cpu_file);
            return 1;
        }

        char mem_buffer[256];
        char cpu_buffer[256];
        char proc_buffer[256];

        while(fgets(mem_buffer, sizeof(mem_buffer), mem_file) != NULL) {
            printf("%s", mem_buffer);
        }
        
        while(fgets(cpu_buffer, sizeof(cpu_buffer), cpu_file) != NULL) {
            printf("%s", cpu_buffer);
        }
        
        while(fgets(proc_buffer, sizeof(proc_buffer), proc_file) != NULL) {
            printf("%s", proc_buffer);
        }

        fclose(mem_file);
        fclose(cpu_file);
        fclose(proc_file);

        usleep(interval);
    }

    return 0;
}