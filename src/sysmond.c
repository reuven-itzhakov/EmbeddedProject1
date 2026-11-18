#include "sysmon_ipc.h"

volatile pid_t pid_mem_sensor;
volatile pid_t pid_cpu_sensor;
volatile pid_t pid_proc_sensor;

void handler(int sig) {
    if (pid_mem_sensor  > 0) kill(pid_mem_sensor,  SIGTERM);
    if (pid_cpu_sensor  > 0) kill(pid_cpu_sensor,  SIGTERM);
    if (pid_proc_sensor > 0) kill(pid_proc_sensor, SIGTERM);
    
    waitpid(pid_mem_sensor, NULL, 0);
    waitpid(pid_cpu_sensor, NULL, 0);
    waitpid(pid_proc_sensor, NULL, 0);

    unlink(PROC_FIFO_PATH);
    unlink(CPU_FIFO_PATH);

    exit(0);
}

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from memory, CPU, and process sensors and displays it.\n");
        return 0;
    }

    int fd[2];
    if(pipe(fd) == -1){
        perror("pipe failed");
        return 1;
    }
    
    pid_mem_sensor = fork();
    if(pid_mem_sensor < 0) {
        perror("Fork failed");
        return 1;
    } else if(pid_mem_sensor == 0) {
        close(fd[0]);
        char *fd_str = malloc(12);
        sprintf(fd_str, "%d", fd[1]);
        execl("./bin/mem_sensor", "./bin/mem_sensor", fd_str, NULL);
        perror("Failed to start mem_sensor");
        return 1;
    }
    close(fd[1]);
    
    pid_cpu_sensor = fork();
    if(pid_cpu_sensor < 0) {
        perror("Fork failed");
        return 1;
    } else if(pid_cpu_sensor == 0) {
        execl("./bin/cpu_sensor", "./bin/cpu_sensor", NULL);
        perror("Failed to start cpu_sensor");
        return 1;
    }

    pid_proc_sensor = fork();
    if(pid_proc_sensor < 0) {
        perror("Fork failed");
        return 1;
    } else if(pid_proc_sensor == 0) {
        execl("./bin/proc_sensor", "./bin/proc_sensor", NULL);
        perror("Failed to start proc_sensor");
        return 1;
    }

    usleep(100000); // Sleep for 100 milliseconds to allow sensors to initialize

    int fd_cpu = open(CPU_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if(fd_cpu == -1) {
        perror("Error opening CPU_FIFO_PATH");
        return 1;
    }

    int fd_proc = open(PROC_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if(fd_proc == -1) {
        perror("Error opening PROC_FIFO_PATH");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = &handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }
    
    FILE* fifo = fdopen(fd[0], "r");
    if(fifo == NULL) {
        perror("Error opening FIFO");
        return 1;
    }

    while(1) {
        system("clear");
        
        MemData mem_data;
        CpuData cpu_data;
        ProcData proc_data;

        if(fread(&mem_data, sizeof(MemData), 1, fifo) > 0) {
            printf("MemTotal: %ld KB\n", mem_data.mem_total);
            printf("MemFree: %ld KB\n", mem_data.mem_free);
            printf("MemAvailable: %ld KB\n", mem_data.mem_avail);
            printf("SwapTotal: %ld KB\n", mem_data.swap_total);
            printf("SwapFree: %ld KB\n", mem_data.swap_free);
        }
        
        if(read(fd_cpu, &cpu_data, sizeof(CpuData)) > 0) {
            printf("Load 1: %.2f\n", cpu_data.load1);
            printf("Load 5: %.2f\n", cpu_data.load5);
            printf("Load 15: %.2f\n", cpu_data.load15);
        }
        
        if(read(fd_proc, &proc_data, sizeof(ProcData)) > 0) {
            printf("Process: %d\n", proc_data.total_procs);
        }

        
        sleep(INTERVAL);
    }
    
    fclose(fifo);
    close(fd[0]);

    return 0;
}