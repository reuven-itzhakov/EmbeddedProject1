#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "sysmon_ipc.h"

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
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from memory, CPU, and process sensors and displays it.\n");
        return 0;
    }


    int fd[2];
    pipe(fd);
    
    pid_t pid_mem_sensor = fork();
    if(pid_mem_sensor < 0) {
        perror("Fork failed");
        return 1;
    } else if(pid_mem_sensor == 0) {
        close(fd[0]);
        execle("./bin/mem_sensor", "./bin/mem_sensor", fd[1], NULL);
        perror("Failed to start mem_sensor");
        return 1;
    }
    close(fd[1]);

    pid_t pid_cpu_sensor = fork();
    if(pid_cpu_sensor < 0) {
        perror("Fork failed");
        return 1;
    } else if(pid_cpu_sensor == 0) {
        execl("./bin/cpu_sensor", "./bin/cpu_sensor", NULL);
        perror("Failed to start cpu_sensor");
        return 1;
    }

    pid_t pid_proc_sensor = fork();
    if(pid_proc_sensor < 0) {
        perror("Fork failed");
        return 1;
    } else if(pid_proc_sensor == 0) {
        execl("./bin/proc_sensor", "./bin/proc_sensor", NULL);
        perror("Failed to start proc_sensor");
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
        FILE* fifo = fdopen(fd[0], "r");
        if(fifo == NULL) {
            perror("Error opening FIFO");
            return 1;
        }
        
        FILE *cpu_file = fopen("/tmp/sysmon/cpu.dat", "r");
        if(cpu_file == NULL) {
            perror("Error opening /tmp/sysmon/cpu.dat");
            fclose(fifo);
            return 1;
        }
        
        FILE *proc_file = fopen("/tmp/sysmon/proc.dat", "r");
        if(proc_file == NULL) {
            perror("Error opening /tmp/sysmon/proc.dat");
            fclose(fifo);
            fclose(cpu_file);
            return 1;
        }

        MemData mem_data;
        char cpu_buffer[256];
        char proc_buffer[256];

        if(fread(&mem_data, sizeof(MemData), 1, fifo) != NULL) {
            printf("MemTotal: %ld KB\n", mem_data.mem_total);
            printf("MemFree: %ld KB\n", mem_data.mem_free);
            printf("MemAvailable: %ld KB\n", mem_data.mem_avail);
            printf("SwapTotal: %ld KB\n", mem_data.swap_total);
            printf("SwapFree: %ld KB\n", mem_data.swap_free);
        }
        
        while(fgets(cpu_buffer, sizeof(cpu_buffer), cpu_file) != NULL) {
            printf("%s", cpu_buffer);
        }
        
        while(fgets(proc_buffer, sizeof(proc_buffer), proc_file) != NULL) {
            printf("%s", proc_buffer);
        }

        fclose(fifo);
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

        sleep(INTERVAL);
    }

    close(fd[0]);

    return 0;
}