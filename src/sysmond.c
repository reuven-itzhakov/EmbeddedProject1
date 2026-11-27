#include <sysmon_ipc.h>

static pid_t mem_pid = -1;
static pid_t cpu_pid = -1;
static pid_t proc_pid = -1;

static mqd_t mem_mqd = (mqd_t)-1;
static mqd_t cpu_mqd = (mqd_t)-1;
static mqd_t proc_mqd = (mqd_t)-1;

static volatile sig_atomic_t running = 1;

void cleanup_all(void) {
    if (mem_pid > 0) {
        kill(mem_pid, SIGTERM);
        waitpid(mem_pid, NULL, 0);
        mem_pid = -1;
    }
    if (cpu_pid > 0) {
        kill(cpu_pid, SIGTERM);
        waitpid(cpu_pid, NULL, 0);
        cpu_pid = -1;
    }
    if (proc_pid > 0) {
        kill(proc_pid, SIGTERM);
        waitpid(proc_pid, NULL, 0);
        proc_pid = -1;
    }

    if (mem_mqd != (mqd_t)-1) {
        mq_close(mem_mqd);
        mem_mqd = (mqd_t)-1;
    }
    if (cpu_mqd != (mqd_t)-1) {
        mq_close(cpu_mqd);
        cpu_mqd = (mqd_t)-1;
    }
    if (proc_mqd != (mqd_t)-1) {
        mq_close(proc_mqd);
        proc_mqd = (mqd_t)-1;
    }

    mq_unlink(MEM_QUEUE_NAME);
    mq_unlink(CPU_QUEUE_NAME);
    mq_unlink(PROC_QUEUE_NAME);
}

void handler(int sig) {
    (void)sig;
    running = 0;
}

int fork_process(pid_t *pid, char *args[]) {
    *pid = fork();
    if (*pid < 0) {
        perror("Fork failed");
        return 1;
    } else if (*pid == 0) {
        if (mem_mqd != (mqd_t)-1) {
            mq_close(mem_mqd);
        }
        if (cpu_mqd != (mqd_t)-1) {
            mq_close(cpu_mqd);
        }
        if (proc_mqd != (mqd_t)-1) {
            mq_close(proc_mqd);
        }

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

    struct mq_attr mem_attr = {0};
    mem_attr.mq_maxmsg = MAX_MESSAGES;
    mem_attr.mq_msgsize = sizeof(MemData);

    struct mq_attr cpu_attr = {0};
    cpu_attr.mq_maxmsg = MAX_MESSAGES;
    cpu_attr.mq_msgsize = sizeof(CpuData);

    struct mq_attr proc_attr = {0};
    proc_attr.mq_maxmsg = MAX_MESSAGES;
    proc_attr.mq_msgsize = sizeof(ProcData);

    mq_unlink(MEM_QUEUE_NAME);
    mq_unlink(CPU_QUEUE_NAME);
    mq_unlink(PROC_QUEUE_NAME);

    mem_mqd = mq_open(MEM_QUEUE_NAME, O_CREAT | O_RDONLY, 0666, &mem_attr);
    if (mem_mqd == (mqd_t)-1) {
        perror("mq_open mem");
        cleanup_all();
        return 1;
    }

    cpu_mqd = mq_open(CPU_QUEUE_NAME, O_CREAT | O_RDONLY, 0666, &cpu_attr);
    if (cpu_mqd == (mqd_t)-1) {
        perror("mq_open cpu");
        cleanup_all();
        return 1;
    }

    proc_mqd = mq_open(PROC_QUEUE_NAME, O_CREAT | O_RDONLY, 0666, &proc_attr);
    if (proc_mqd == (mqd_t)-1) {
        perror("mq_open proc");
        cleanup_all();
        return 1;
    }

    char *mem_args[] = {"./bin/mem_sensor", NULL};
    if (fork_process(&mem_pid, mem_args) != 0) {
        cleanup_all();
        return 1;
    }

    char *cpu_args[] = {"./bin/cpu_sensor", NULL};
    if (fork_process(&cpu_pid, cpu_args) != 0) {
        cleanup_all();
        return 1;
    }

    char *proc_args[] = {"./bin/proc_sensor", NULL};
    if (fork_process(&proc_pid, proc_args) != 0) {
        cleanup_all();
        return 1;
    }

    while (running) {
        MemData mem_data = {0};
        CpuData cpu_data = {0};
        ProcData proc_data = {0};

        unsigned int mem_prio = 0;
        unsigned int cpu_prio = 0;
        unsigned int proc_prio = 0;

        if (mq_receive(mem_mqd, (char *)&mem_data, sizeof(MemData), &mem_prio) == -1) {
            if (!running || errno == EINTR) {
                break;
            }
            perror("mq_receive mem");
            break;
        }

        if (mq_receive(cpu_mqd, (char *)&cpu_data, sizeof(CpuData), &cpu_prio) == -1) {
            if (!running || errno == EINTR) {
                break;
            }
            perror("mq_receive cpu");
            break;
        }

        if (mq_receive(proc_mqd, (char *)&proc_data, sizeof(ProcData), &proc_prio) == -1) {
            if (!running || errno == EINTR) {
                break;
            }
            perror("mq_receive proc");
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
        printf("Memory (PID %d) priority: %u\n", mem_pid, mem_prio);
        printf("CPU (PID %d) priority: %u\n", cpu_pid, cpu_prio);
        printf("Process (PID %d) priority: %u\n", proc_pid, proc_prio);
        fflush(stdout);
    }

    cleanup_all();
    return 0;
}