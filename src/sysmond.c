#include "sysmon_ipc.h"

volatile pid_t pid_mem_sensor = 0;
volatile pid_t pid_cpu_sensor = 0;
volatile pid_t pid_proc_sensor = 0;

volatile int fd_pipe = -1;
volatile int fd_cpu = -1;
volatile int fd_proc = -1;

void handler(int sig) {
    if (pid_mem_sensor  > 0) kill(pid_mem_sensor,  SIGTERM);
    if (pid_cpu_sensor  > 0) kill(pid_cpu_sensor,  SIGTERM);
    if (pid_proc_sensor > 0) kill(pid_proc_sensor, SIGTERM);
    
    if (pid_mem_sensor  > 0) waitpid(pid_mem_sensor,  NULL, 0);
    if (pid_cpu_sensor  > 0) waitpid(pid_cpu_sensor,  NULL, 0);
    if (pid_proc_sensor > 0) waitpid(pid_proc_sensor, NULL, 0);

    if (fd_pipe >= 0) close(fd_pipe);
    if (fd_cpu  >= 0) close(fd_cpu);
    if (fd_proc >= 0) close(fd_proc);

    unlink(PROC_FIFO_PATH);
    unlink(CPU_FIFO_PATH);

    exit(0);
}

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s <sysmon_interval> <mem_interval> <cpu_interval> <proc_interval>\n", argv[0]);
        printf("This program reads data from memory, CPU, and process sensors and displays it.\n");
        printf("The sensors gets interval values in seconds from the command line arguments, if not provided, default values are used (1).\n");
        return 0;
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

    char* interval_str[4];
    for(int i = 0; i < 4; i++) {
        if(argc > i + 1) {
            interval_str[i] = argv[i + 1];
        } else {
            interval_str[i] = "1";
        }
    }

    int interval;
    if(argc > 1 && atoi(argv[1]) > 0) {
        interval = atoi(argv[1]);
    } else {
        interval = INTERVAL;
    }

    if(mkfifo(CPU_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating CPU_FIFO_PATH");
        return 1;
    }

    if(mkfifo(PROC_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating PROC_FIFO_PATH");
        return 1;
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
        char fd_str[16];
        snprintf(fd_str, sizeof(fd_str), "%d", fd[1]);
        execl("./bin/mem_sensor", "./bin/mem_sensor", fd_str, interval_str[1], NULL);
        perror("Failed to start mem_sensor");
        _exit(1);
    }
    close(fd[1]);

    fd_pipe = fd[0];
    fcntl(fd_pipe, F_SETFL, fcntl(fd_pipe, F_GETFL, 0) | O_NONBLOCK);
    
    pid_cpu_sensor = fork();
    if(pid_cpu_sensor < 0) {
        perror("Fork failed");
        return 1;
    } else if(pid_cpu_sensor == 0) {
        execl("./bin/cpu_sensor", "./bin/cpu_sensor", interval_str[2], NULL);
        perror("Failed to start cpu_sensor");
        _exit(1);
    }

    pid_proc_sensor = fork();
    if(pid_proc_sensor < 0) {
        perror("Fork failed");
        return 1;
    } else if(pid_proc_sensor == 0) {
        execl("./bin/proc_sensor", "./bin/proc_sensor", interval_str[3], NULL);
        perror("Failed to start proc_sensor");
        _exit(1);
    }

    fd_cpu = open(CPU_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if(fd_cpu == -1) {
        perror("Error opening CPU_FIFO_PATH");
        return 1;
    }

    fd_proc = open(PROC_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if(fd_proc == -1) {
        perror("Error opening PROC_FIFO_PATH");
        return 1;
    }

    FILE* fifo = fdopen(fd[0], "r");
    if(fifo == NULL) {
        perror("Error opening FIFO");
        return 1;
    }
    int fdFifo = fileno(fifo);
    int flags = fcntl(fdFifo, F_GETFL, 0);
    fcntl(fdFifo, F_SETFL, flags | O_NONBLOCK);

    struct pollfd fds[3];
    fds[0].fd = fd_pipe;
    fds[0].events = POLLIN;
    fds[1].fd = fd_cpu;
    fds[1].events = POLLIN;
    fds[2].fd = fd_proc;
    fds[2].events = POLLIN;

    MemData mem_data = {0};
    CpuData cpu_data = {0};
    ProcData proc_data = {0};

    while(1) {
        int ret = poll(fds, 3, interval * 1000);
        if(ret < 0) {
            if(errno == EINTR) {
                continue;
            }
            perror("poll failed");
            break;
        }

        if(fds[0].fd >= 0 && (fds[0].revents & POLLIN)) {
            MemData temp_mem;
            ssize_t bytes;
            while((bytes = read(fd_pipe, &temp_mem, sizeof(MemData))) == sizeof(MemData)) {
                mem_data = temp_mem;
            }
            if(bytes == 0) {
                fds[0].fd = -1;
            }
        } else if(fds[0].fd >= 0 && (fds[0].revents & POLLHUP)) {
            fds[0].fd = -1;
        }

        if(fds[1].fd >= 0 && (fds[1].revents & POLLIN)) {
            CpuData temp_cpu;
            ssize_t bytes;
            while((bytes = read(fd_cpu, &temp_cpu, sizeof(CpuData))) == sizeof(CpuData)) {
                cpu_data = temp_cpu;
            }
            if(bytes == 0) {
                fds[1].fd = -1;
            }
        } else if(fds[1].fd >= 0 && (fds[1].revents & POLLHUP)) {
            fds[1].fd = -1;
        }

        if(fds[2].fd >= 0 && (fds[2].revents & POLLIN)) {
            ProcData temp_proc;
            ssize_t bytes;
            while((bytes = read(fd_proc, &temp_proc, sizeof(ProcData))) == sizeof(ProcData)) {
                proc_data = temp_proc;
            }
            if(bytes == 0) {
                fds[2].fd = -1;
            }
        } else if(fds[2].fd >= 0 && (fds[2].revents & POLLHUP)) {
            fds[2].fd = -1;
        }

        if(fds[0].fd == -1 && fds[1].fd == -1 && fds[2].fd == -1) {
            break;
        }

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
    }
    
    handler(SIGTERM);
    return 0;
}
