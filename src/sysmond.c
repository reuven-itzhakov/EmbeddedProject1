#include "sysmon_ipc.h"

struct SensorState {
    volatile pid_t pid;
    int fd_pipe[2];
    int fd;
    int restarts;
    int status;
    int terminated_by_signal;
};

struct SensorState mem_sensor = {
    .pid = 0, 
    .fd_pipe = {-1, -1}, 
    .fd = -1, 
    .restarts = 0, 
    .status = 0
};
struct SensorState cpu_sensor = {
    .pid = 0, 
    .fd_pipe = {-1, -1}, 
    .fd = -1, 
    .restarts = 0, 
    .status = 0
};
struct SensorState proc_sensor = {
    .pid = 0, 
    .fd_pipe = {-1, -1}, 
    .fd = -1, 
    .restarts = 0, 
    .status = 0
};

volatile sig_atomic_t running = 1;
char last_msg[128];

void handler(int sig) {
    (void)sig;
    running = 0;
}

int close_process(struct SensorState* sensor) {
    int status = 0;
    if (sensor->pid > 0) {
        kill(sensor->pid, SIGTERM);
        waitpid(sensor->pid, &status, 0);
        sensor->pid = 0;
    }
    if (sensor->fd_pipe[0] != -1) {
        close(sensor->fd_pipe[0]);
        sensor->fd_pipe[0] = -1;
    }
    if (sensor->fd_pipe[1] != -1) {
        close(sensor->fd_pipe[1]);
        sensor->fd_pipe[1] = -1;
    }
    if (sensor->fd != -1) {
        close(sensor->fd);
        sensor->fd = -1;
    }

    sensor->terminated_by_signal = 0;
sensor->status = 0;

    if (WIFSIGNALED(status)) {
        sensor->terminated_by_signal = 1;
        sensor->status = WTERMSIG(status);
    } else if (WIFEXITED(status)) {
        sensor->terminated_by_signal = 0;
        sensor->status = WEXITSTATUS(status);
    }

    return sensor->status;
}

static void cleanup_all(void) {
    close_process(&mem_sensor);
    close_process(&cpu_sensor);
    close_process(&proc_sensor);

    unlink(PROC_FIFO_PATH);
    unlink(CPU_FIFO_PATH);
}

int fork_process(volatile pid_t* pid, char* args[]) {
    *pid = fork();
    if (*pid < 0) {
        perror("Fork failed");
        cleanup_all();
        return 1;
    } else if (*pid == 0) {
        close(mem_sensor.fd_pipe[0]);
        close(cpu_sensor.fd);
        close(proc_sensor.fd);
        execve(args[0], args, NULL);
        perror("Failed to start sensor");
        _exit(1);
    }
    return 0;
}

int create_fifo(const char* path) {
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        char error_message[128];
        snprintf(error_message, sizeof(error_message), "Error creating %s", path);
        perror(error_message);
        cleanup_all();
        return 1;
    }
    return 0;
}

int open_fifo(int* fd, const char* path, int flags) {
    *fd = open(path, flags);
    if (*fd == -1) {
        char error_message[128];
        snprintf(error_message, sizeof(error_message), "Error opening %s", path);
        perror(error_message);
        cleanup_all();
        return 1;
    }
    return 0;
}

int restart_process(struct SensorState* sensor, char* args[], char* buffer, size_t buffer_size, int is_fifo) {
    int pid = sensor->pid;
    close_process(sensor);
    
    if(is_fifo) {
        if(fork_process(&sensor->pid, args) != 0) {
            return 1;
        }
        if(open_fifo(&sensor->fd, buffer, O_RDONLY) != 0) {
            return 1;
        }
    } else {
        if(pipe(sensor->fd_pipe) == -1) {
            perror("pipe failed");
            cleanup_all();
            return 1;
        }
        snprintf(buffer, buffer_size, "%d", sensor->fd_pipe[1]);
        args[1] = buffer;
        if(fork_process(&sensor->pid, args) != 0) {
            return 1;
        }
        close(sensor->fd_pipe[1]);
        sensor->fd_pipe[1] = -1;
    }

    sensor->restarts++;

    if (sensor->terminated_by_signal) {
    snprintf(last_msg, sizeof(last_msg),
             "Sensor (PID %d) killed by signal %d. Restarted %d times.\n",
             pid, sensor->status, sensor->restarts);
    } else {
    snprintf(last_msg, sizeof(last_msg),
             "Sensor (PID %d) exited with code %d. Restarted %d times.\n",
             pid, sensor->status, sensor->restarts);
}

    return 0;
}

static ssize_t read_exact(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *ptr = buf;
    while (total < count) {
        if (!running) {
            errno = EINTR;
            return -1;
        }
        ssize_t n = read(fd, ptr + total, count - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
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

    if (pipe(mem_sensor.fd_pipe) == -1) {
        perror("pipe failed");
        return 1;
    }

    if (create_fifo(CPU_FIFO_PATH) != 0) {
        return 1;
    }

    if (create_fifo(PROC_FIFO_PATH) != 0) {
        return 1;
    }

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d", mem_sensor.fd_pipe[1]);
    char* mem_sensor_args[] = {"./bin/mem_sensor", buffer, NULL};
    if (fork_process(&mem_sensor.pid, mem_sensor_args) != 0) {
        return 1;
    }
    close(mem_sensor.fd_pipe[1]);
    mem_sensor.fd_pipe[1] = -1;

    char* cpu_sensor_args[] = {"./bin/cpu_sensor", NULL};
    if (fork_process(&cpu_sensor.pid, cpu_sensor_args) != 0) {
        return 1;
    }

    char* proc_sensor_args[] = {"./bin/proc_sensor", NULL};
    if (fork_process(&proc_sensor.pid, proc_sensor_args) != 0) {
        return 1;
    }

    if(open_fifo(&cpu_sensor.fd, CPU_FIFO_PATH, O_RDONLY) != 0) {
        return 1;
    }
    if(open_fifo(&proc_sensor.fd, PROC_FIFO_PATH, O_RDONLY) != 0) {
        return 1;
    }

    while (running) {
        MemData mem_data = {0};
        CpuData cpu_data = {0};
        ProcData proc_data = {0};

        if (running && read_exact(mem_sensor.fd_pipe[0], &mem_data, sizeof(MemData)) <= 0) {
            restart_process(&mem_sensor, mem_sensor_args, buffer, sizeof(buffer), 0);
            continue;
        }
        if (running && read_exact(cpu_sensor.fd, &cpu_data, sizeof(CpuData)) <= 0) {
            restart_process(&cpu_sensor, cpu_sensor_args, CPU_FIFO_PATH, 0, 1);
            continue;
        }
        if (running && read_exact(proc_sensor.fd, &proc_data, sizeof(ProcData)) <= 0) {
            restart_process(&proc_sensor, proc_sensor_args, PROC_FIFO_PATH, 0, 1);
            continue;
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
        if(mem_sensor.restarts > 0) {
            printf("Mem Sensor (PID %d) %s by signal %d. Restarted %d times.\n", mem_sensor.pid, mem_sensor.terminated_by_signal ? "killed" : "exited", mem_sensor.status, mem_sensor.restarts);
        }
        if(cpu_sensor.restarts > 0) {
            printf("CPU Sensor (PID %d) %s by signal %d. Restarted %d times.\n", cpu_sensor.pid, cpu_sensor.terminated_by_signal ? "killed" : "exited", cpu_sensor.status, cpu_sensor.restarts);
        }
        if(proc_sensor.restarts > 0) {
            printf("Process Sensor (PID %d) %s by signal %d. Restarted %d times.\n", proc_sensor.pid, proc_sensor.terminated_by_signal ? "killed" : "exited", proc_sensor.status, proc_sensor.restarts);
        }
        fflush(stdout);
    }

    cleanup_all();
    return 0;
}
