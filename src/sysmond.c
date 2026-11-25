#include "sysmon_ipc.h"

volatile pid_t pid_mem_sensor = 0;
volatile pid_t pid_cpu_sensor = 0;
volatile pid_t pid_proc_sensor = 0;

static int fd_pipe[2] = {-1, -1};
static int fd_cpu = -1;
static int fd_proc = -1;

static void cleanup_children_and_fifos(void) {
    if (pid_mem_sensor > 0) kill(pid_mem_sensor, SIGTERM);
    if (pid_cpu_sensor > 0) kill(pid_cpu_sensor, SIGTERM);
    if (pid_proc_sensor > 0) kill(pid_proc_sensor, SIGTERM);

    if (pid_mem_sensor > 0) waitpid(pid_mem_sensor, NULL, 0);
    if (pid_cpu_sensor > 0) waitpid(pid_cpu_sensor, NULL, 0);
    if (pid_proc_sensor > 0) waitpid(pid_proc_sensor, NULL, 0);

    unlink(PROC_FIFO_PATH);
    unlink(CPU_FIFO_PATH);
}

void handler(int sig) {
    cleanup_children_and_fifos();
    _exit(0);
}

static void cleanup_all(void) {
    cleanup_children_and_fifos();
    if (fd_pipe[0] != -1) close(fd_pipe[0]);
    if (fd_pipe[1] != -1) close(fd_pipe[1]);
    if (fd_cpu != -1) close(fd_cpu);
    if (fd_proc != -1) close(fd_proc);
}

static ssize_t read_exact(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *ptr = buf;
    while (total < count) {
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

    if (pipe(fd_pipe) == -1) {
        perror("pipe failed");
        return 1;
    }

    if (mkfifo(CPU_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating CPU_FIFO_PATH");
        cleanup_all();
        return 1;
    }

    if (mkfifo(PROC_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating PROC_FIFO_PATH");
        cleanup_all();
        return 1;
    }

    pid_mem_sensor = fork();
    if (pid_mem_sensor < 0) {
        perror("Fork failed");
        cleanup_all();
        return 1;
    } else if (pid_mem_sensor == 0) {
        close(fd_pipe[0]);
        char fd_str[16];
        snprintf(fd_str, sizeof(fd_str), "%d", fd_pipe[1]);
        execl("./bin/mem_sensor", "./bin/mem_sensor", fd_str, NULL);
        perror("Failed to start mem_sensor");
        _exit(1);
    }
    close(fd_pipe[1]);
    fd_pipe[1] = -1;

    pid_cpu_sensor = fork();
    if (pid_cpu_sensor < 0) {
        perror("Fork failed");
        cleanup_all();
        return 1;
    } else if (pid_cpu_sensor == 0) {
        close(fd_pipe[0]);
        execl("./bin/cpu_sensor", "./bin/cpu_sensor", NULL);
        perror("Failed to start cpu_sensor");
        _exit(1);
    }

    pid_proc_sensor = fork();
    if (pid_proc_sensor < 0) {
        perror("Fork failed");
        cleanup_all();
        return 1;
    } else if (pid_proc_sensor == 0) {
        close(fd_pipe[0]);
        execl("./bin/proc_sensor", "./bin/proc_sensor", NULL);
        perror("Failed to start proc_sensor");
        _exit(1);
    }


    fd_cpu = open(CPU_FIFO_PATH, O_RDONLY);
    if (fd_cpu == -1) {
        perror("Error opening CPU_FIFO_PATH");
        cleanup_all();
        return 1;
    }

    fd_proc = open(PROC_FIFO_PATH, O_RDONLY);
    if (fd_proc == -1) {
        perror("Error opening PROC_FIFO_PATH");
        cleanup_all();
        return 1;
    }

    while (1) {
        MemData mem_data = {0};
        CpuData cpu_data = {0};
        ProcData proc_data = {0};

        ssize_t r_mem = read_exact(fd_pipe[0], &mem_data, sizeof(MemData));
        ssize_t r_cpu = read_exact(fd_cpu, &cpu_data, sizeof(CpuData));
        ssize_t r_proc = read_exact(fd_proc, &proc_data, sizeof(ProcData));

        if (r_mem <= 0 || r_cpu <= 0 || r_proc <= 0) {
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
        fflush(stdout);
    }

    cleanup_all();
    return 0;
}
