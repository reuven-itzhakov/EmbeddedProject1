#include "sysmon_ipc.h"

volatile pid_t pid_mem_sensor;
volatile pid_t pid_cpu_sensor;
volatile pid_t pid_proc_sensor;

volatile sig_atomic_t running = 1;

void handler(int sig) {
    (void)sig;
    running = 0;
}

int fork_process(volatile pid_t* pid, char* args[]) {
    *pid = fork();
    if (*pid < 0) {
        perror("Fork failed");
        cleanup_all();
        return 1;
    } else if (*pid == 0) {
        execve(args[0], args, NULL);
        perror("Failed to start sensor");
        _exit(1);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from memory, CPU, and process sensors and displays it.\n");
        return 0;
    }

    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }

    char* mem_sensor_args[] = {"./bin/mem_sensor", NULL};
    if (fork_process(&pid_mem_sensor, mem_sensor_args) != 0) {
        return 1;
    }

    char* cpu_sensor_args[] = {"./bin/cpu_sensor", NULL};
    if (fork_process(&pid_cpu_sensor, cpu_sensor_args) != 0) {
        return 1;
    }

    char* proc_sensor_args[] = {"./bin/proc_sensor", NULL};
    if (fork_process(&pid_proc_sensor, proc_sensor_args) != 0) {
        return 1;
    }

    while (running) {
        MemData mem_data = {0};
        CpuData cpu_data = {0};
        ProcData proc_data = {0};

        system("clear");
        printf("MemTotal: %ld KB\n", mem_data.mem_total);
        printf("MemFree: %ld KB\n", mem_data.mem_free);
        printf("MemAvailable: %ld KB\n", mem_data.mem_avail);
        printf("SwapTotal: %ld KB\n", mem_data.swap_total);
        printf("SwapFree: %ld KB\n", mem_data.swap_free);
        printf("Load 1: %.2f\n", cpu_data.load1);
        printf("Load 5: %.2f\n", cpu_data.load5);
        printf("Load 15: %.2f\n", cpu_data.load15);
        printf("Process: %d\n", proc_data.total_procs);
        fflush(stdout);
    }

    cleanup_all();
    return 0;
}
