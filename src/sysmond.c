#include "sysmon_ipc.h"

volatile pid_t pids[3] = {0};
volatile int fds[3] = {-1, -1, -1};

void handler(int sig) {
    for(int i = 0; i < 3; i++) {
        if(pids[i] > 0) {
            kill(pids[i], SIGTERM);
        }
    }
    for(int i = 0; i < 3; i++) {
        if(pids[i] > 0) {
            waitpid(pids[i], NULL, 0);
        }
    }
    for(int i = 0; i < 3; i++) {
        if(fds[i] >= 0) {
            close(fds[i]);
        }
    }
    unlink(PROC_FIFO_PATH);
    unlink(CPU_FIFO_PATH);
    exit(0);
}

static int setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = &handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction failed");
        return -1;
    }
    return 0;
}

static int create_fifos(void) {
    const char *paths[2] = {CPU_FIFO_PATH, PROC_FIFO_PATH};
    for(int i = 0; i < 2; i++) {
        if(mkfifo(paths[i], 0666) == -1 && errno != EEXIST) {
            perror("Error creating FIFO");
            return -1;
        }
    }
    return 0;
}

static int spawn_sensors(char *interval_str[]) {
    int pipe_fd[2];
    if(pipe(pipe_fd) == -1) {
        perror("pipe failed");
        return -1;
    }

    pids[0] = fork();
    if(pids[0] < 0) {
        perror("Fork failed");
        return -1;
    } else if(pids[0] == 0) {
        close(pipe_fd[0]);
        char fd_str[16];
        snprintf(fd_str, sizeof(fd_str), "%d", pipe_fd[1]);
        execl("./bin/mem_sensor", "./bin/mem_sensor", fd_str, interval_str[1], NULL);
        perror("Failed to start mem_sensor");
        _exit(1);
    }
    close(pipe_fd[1]);

    fds[0] = pipe_fd[0];
    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);

    const char *binaries[2] = {"./bin/cpu_sensor", "./bin/proc_sensor"};
    for(int i = 0; i < 2; i++) {
        pids[i + 1] = fork();
        if(pids[i + 1] < 0) {
            perror("Fork failed");
            return -1;
        } else if(pids[i + 1] == 0) {
            execl(binaries[i], binaries[i], interval_str[i + 2], NULL);
            perror("Failed to start sensor");
            _exit(1);
        }
    }
    return 0;
}

static int open_fifos(void) {
    const char *paths[2] = {CPU_FIFO_PATH, PROC_FIFO_PATH};
    for(int i = 0; i < 2; i++) {
        fds[i + 1] = open(paths[i], O_RDONLY | O_NONBLOCK);
        if(fds[i + 1] == -1) {
            perror("Error opening FIFO");
            return -1;
        }
    }
    return 0;
}

static void drain_fd(struct pollfd *pfd, void *dest, size_t size) {
    if(pfd->fd < 0) {
        return;
    }
    if(pfd->revents & POLLIN) {
        char buf[sizeof(MemData)];
        ssize_t bytes;
        while((bytes = read(pfd->fd, buf, size)) == (ssize_t)size) {
            memcpy(dest, buf, size);
        }
        if(bytes == 0) {
            pfd->fd = -1;
        }
    } else if(pfd->revents & POLLHUP) {
        pfd->fd = -1;
    }
}

static void render_dashboard(const MemData *mem, const CpuData *cpu, const ProcData *proc) {
    system("clear");

    printf("MemTotal: %ld KB\n", mem->mem_total);
    printf("MemFree: %ld KB\n", mem->mem_free);
    printf("MemAvailable: %ld KB\n", mem->mem_avail);
    printf("SwapTotal: %ld KB\n", mem->swap_total);
    printf("SwapFree: %ld KB\n", mem->swap_free);
    
    printf("Load 1: %.2f\n", cpu->load1);
    printf("Load 5: %.2f\n", cpu->load5);
    printf("Load 15: %.2f\n", cpu->load15);
    
    printf("Process: %d\n", proc->total_procs);
}

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s <sysmon_interval> <mem_interval> <cpu_interval> <proc_interval>\n", argv[0]);
        printf("This program reads data from memory, CPU, and process sensors and displays it.\n");
        printf("The sensors gets interval values in seconds from the command line arguments, if not provided, default values are used (1).\n");
        return 0;
    }

    if(setup_signals() == -1) {
        return 1;
    }

    char* interval_str[4];
    for(int i = 0; i < 4; i++) {
        interval_str[i] = (argc > i + 1) ? argv[i + 1] : "1";
    }

    int interval = (argc > 1 && atoi(argv[1]) > 0) ? atoi(argv[1]) : INTERVAL;

    if(create_fifos() == -1) {
        return 1;
    }

    if(spawn_sensors(interval_str) == -1) {
        return 1;
    }

    if(open_fifos() == -1) {
        return 1;
    }

    struct pollfd pfds[3];
    for(int i = 0; i < 3; i++) {
        pfds[i].fd = fds[i];
        pfds[i].events = POLLIN;
    }

    MemData mem_data = {0};
    CpuData cpu_data = {0};
    ProcData proc_data = {0};

    while(1) {
        int ret = poll(pfds, 3, interval * 1000);
        if(ret < 0) {
            if(errno == EINTR) {
                continue;
            }
            perror("poll failed");
            break;
        }

        drain_fd(&pfds[0], &mem_data, sizeof(MemData));
        drain_fd(&pfds[1], &cpu_data, sizeof(CpuData));
        drain_fd(&pfds[2], &proc_data, sizeof(ProcData));

        if(pfds[0].fd == -1 && pfds[1].fd == -1 && pfds[2].fd == -1) {
            break;
        }

        render_dashboard(&mem_data, &cpu_data, &proc_data);
    }
    
    handler(SIGTERM);
    return 0;
}
