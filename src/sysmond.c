#include "sysmon_ipc.h"

static volatile sig_atomic_t running = 1;

static volatile pid_t pid_mem_sensor = 0;
static volatile pid_t pid_cpu_sensor = 0;
static volatile pid_t pid_proc_sensor = 0;

static int fd_pipe[2] = {-1, -1};
static int fd_cpu = -1;
static int fd_proc = -1;

static MemData shared_mem = {0};
static CpuData shared_cpu = {0};
static ProcData shared_proc = {0};
static int data_updated = 0;

static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t data_cond = PTHREAD_COND_INITIALIZER;

static void cleanup_children_and_fifos(void) {
    if (pid_mem_sensor > 0) kill(pid_mem_sensor, SIGTERM);
    if (pid_cpu_sensor > 0) kill(pid_cpu_sensor, SIGTERM);
    if (pid_proc_sensor > 0) kill(pid_proc_sensor, SIGTERM);

    if (pid_mem_sensor > 0) {
        waitpid(pid_mem_sensor, NULL, 0);
        pid_mem_sensor = 0;
    }
    if (pid_cpu_sensor > 0) {
        waitpid(pid_cpu_sensor, NULL, 0);
        pid_cpu_sensor = 0;
    }
    if (pid_proc_sensor > 0) {
        waitpid(pid_proc_sensor, NULL, 0);
        pid_proc_sensor = 0;
    }

    unlink(PROC_FIFO_PATH);
    unlink(CPU_FIFO_PATH);
}

void handler(int sig) {
    running = 0;
    if (pid_mem_sensor > 0) kill(pid_mem_sensor, SIGTERM);
    if (pid_cpu_sensor > 0) kill(pid_cpu_sensor, SIGTERM);
    if (pid_proc_sensor > 0) kill(pid_proc_sensor, SIGTERM);
}

static void cleanup_all(void) {
    cleanup_children_and_fifos();
    if (fd_pipe[0] != -1) {
        close(fd_pipe[0]);
        fd_pipe[0] = -1;
    }
    if (fd_pipe[1] != -1) {
        close(fd_pipe[1]);
        fd_pipe[1] = -1;
    }
    if (fd_cpu != -1) {
        close(fd_cpu);
        fd_cpu = -1;
    }
    if (fd_proc != -1) {
        close(fd_proc);
        fd_proc = -1;
    }
}

static ssize_t read_exact(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *ptr = buf;
    while (total < count) {
        ssize_t n = read(fd, ptr + total, count - total);
        if (n < 0) {
            if (errno == EINTR) {
                if (!running) {
                    return 0;
                }
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

static int parse_interval(const char *arg, int default_val) {
    char *endptr = NULL;
    errno = 0;
    long val = strtol(arg, &endptr, 10);
    if (errno != 0 || endptr == arg || *endptr != '\0' || val <= 0) {
        return default_val;
    }
    return (int)val;
}

static void *mem_reader_thread(void *arg) {
    while (running) {
        MemData local_mem = {0};
        ssize_t r = read_exact(fd_pipe[0], &local_mem, sizeof(MemData));
        if (r <= 0) {
            running = 0;
            pthread_mutex_lock(&data_mutex);
            pthread_cond_broadcast(&data_cond);
            pthread_mutex_unlock(&data_mutex);
            break;
        }

        pthread_mutex_lock(&data_mutex);
        shared_mem = local_mem;
        data_updated = 1;
        pthread_cond_signal(&data_cond);
        pthread_mutex_unlock(&data_mutex);
    }
    return NULL;
}

static void *cpu_reader_thread(void *arg) {
    while (running) {
        CpuData local_cpu = {0};
        ssize_t r = read_exact(fd_cpu, &local_cpu, sizeof(CpuData));
        if (r <= 0) {
            running = 0;
            pthread_mutex_lock(&data_mutex);
            pthread_cond_broadcast(&data_cond);
            pthread_mutex_unlock(&data_mutex);
            break;
        }

        pthread_mutex_lock(&data_mutex);
        shared_cpu = local_cpu;
        data_updated = 1;
        pthread_cond_signal(&data_cond);
        pthread_mutex_unlock(&data_mutex);
    }
    return NULL;
}

static void *proc_reader_thread(void *arg) {
    while (running) {
        ProcData local_proc = {0};
        ssize_t r = read_exact(fd_proc, &local_proc, sizeof(ProcData));
        if (r <= 0) {
            running = 0;
            pthread_mutex_lock(&data_mutex);
            pthread_cond_broadcast(&data_cond);
            pthread_mutex_unlock(&data_mutex);
            break;
        }

        pthread_mutex_lock(&data_mutex);
        shared_proc = local_proc;
        data_updated = 1;
        pthread_cond_signal(&data_cond);
        pthread_mutex_unlock(&data_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [mem_interval] [cpu_interval] [proc_interval]\n", argv[0]);
        printf("This program reads data from memory, CPU, and process sensors and displays it.\n");
        return 0;
    }

    int mem_interval = (argc > 1) ? parse_interval(argv[1], MEM_INTERVAL) : MEM_INTERVAL;
    int cpu_interval = (argc > 2) ? parse_interval(argv[2], CPU_INTERVAL) : CPU_INTERVAL;
    int proc_interval = (argc > 3) ? parse_interval(argv[3], PROC_INTERVAL) : PROC_INTERVAL;

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
        char interval_str[16];
        snprintf(fd_str, sizeof(fd_str), "%d", fd_pipe[1]);
        snprintf(interval_str, sizeof(interval_str), "%d", mem_interval);
        execl("./bin/mem_sensor", "./bin/mem_sensor", fd_str, interval_str, (char *)NULL);
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
        char interval_str[16];
        snprintf(interval_str, sizeof(interval_str), "%d", cpu_interval);
        execl("./bin/cpu_sensor", "./bin/cpu_sensor", interval_str, (char *)NULL);
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
        char interval_str[16];
        snprintf(interval_str, sizeof(interval_str), "%d", proc_interval);
        execl("./bin/proc_sensor", "./bin/proc_sensor", interval_str, (char *)NULL);
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

    pthread_t th_mem, th_cpu, th_proc;
    if (pthread_create(&th_mem, NULL, mem_reader_thread, NULL) != 0) {
        perror("Failed to create mem_reader_thread");
        cleanup_all();
        return 1;
    }
    if (pthread_create(&th_cpu, NULL, cpu_reader_thread, NULL) != 0) {
        perror("Failed to create cpu_reader_thread");
        running = 0;
        cleanup_all();
        pthread_join(th_mem, NULL);
        return 1;
    }
    if (pthread_create(&th_proc, NULL, proc_reader_thread, NULL) != 0) {
        perror("Failed to create proc_reader_thread");
        running = 0;
        cleanup_all();
        pthread_join(th_mem, NULL);
        pthread_join(th_cpu, NULL);
        return 1;
    }

    while (running) {
        pthread_mutex_lock(&data_mutex);
        while (running && !data_updated) {
            pthread_cond_wait(&data_cond, &data_mutex);
        }
        if (!running) {
            pthread_mutex_unlock(&data_mutex);
            break;
        }

        MemData mem_snap = shared_mem;
        CpuData cpu_snap = shared_cpu;
        ProcData proc_snap = shared_proc;
        data_updated = 0;
        pthread_mutex_unlock(&data_mutex);

        system("clear");
        printf("MemTotal: %ld KB\n", mem_snap.mem_total);
        printf("MemFree: %ld KB\n", mem_snap.mem_free);
        printf("MemAvailable: %ld KB\n", mem_snap.mem_avail);
        printf("SwapTotal: %ld KB\n", mem_snap.swap_total);
        printf("SwapFree: %ld KB\n", mem_snap.swap_free);
        printf("Load 1: %.2f\n", cpu_snap.load1);
        printf("Load 5: %.2f\n", cpu_snap.load5);
        printf("Load 15: %.2f\n", cpu_snap.load15);
        printf("Process: %d\n", proc_snap.total_procs);
        fflush(stdout);
    }

    cleanup_all();
    pthread_join(th_mem, NULL);
    pthread_join(th_cpu, NULL);
    pthread_join(th_proc, NULL);

    pthread_mutex_destroy(&data_mutex);
    pthread_cond_destroy(&data_cond);
    return 0;
}