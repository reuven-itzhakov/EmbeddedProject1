#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

sig_atomic_t child_exited = 0;
sig_atomic_t child_pid = 0;
sig_atomic_t child_status = 0;

void handle_sigchld(int sig) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        child_exited = 1;
        child_pid = pid;
        child_status = status;
    }
}

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
    
    char interval_str[32];
    snprintf(interval_str, sizeof(interval_str), "%.2f", (double)interval / 1000000.0);

    pthread_t tid_mem_sensor;
    if(pthread_create(&tid_mem_sensor, NULL, mem_sensor, (void*)&interval) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    pthread_t tid_cpu_sensor;
    if(pthread_create(&tid_cpu_sensor, NULL, cpu_sensor, (void*)&interval) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    pthread_t tid_proc_sensor;
    if(pthread_create(&tid_proc_sensor, NULL, proc_sensor, (void*)&interval) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }

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

        if (child_exited) {
            if (WIFEXITED(child_status)) {
                printf("\nChild process (PID: %d) exited normally with status %d\n", child_pid, WEXITSTATUS(child_status));
            } else if (WIFSIGNALED(child_status)) {
                printf("\nChild process (PID: %d) was killed by signal %d\n", child_pid, WTERMSIG(child_status));
            }
            child_exited = 0;
        }

        usleep(interval);
    }

    return 0;
}