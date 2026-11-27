#include <sysmon_ipc.h>

static volatile sig_atomic_t running = 1;

void handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from CPU sensor and sends it to the CPU message queue.\n");
        return 0;
    }

    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting signal handler");
        return 1;
    }

    mqd_t mqd = mq_open(CPU_QUEUE_NAME, O_WRONLY);
    if (mqd == (mqd_t)-1) {
        perror("mq_open cpu");
        return 1;
    }

    while (running) {
        FILE *file = fopen("/proc/loadavg", "r");
        if (file == NULL) {
            perror("Error opening /proc/loadavg");
            mq_close(mqd);
            return 1;
        }

        CpuData cpu_data = {0};
        int parsed = fscanf(file, "%lf %lf %lf", &cpu_data.load1, &cpu_data.load5, &cpu_data.load15);
        fclose(file);

        if (parsed != 3) {
            fprintf(stderr, "Error reading load averages\n");
            sleep(INTERVAL);
            continue;
        }

        unsigned int priority = (cpu_data.load1 > 1.50) ? 10 : 1;

        if (mq_send(mqd, (const char *)&cpu_data, sizeof(CpuData), priority) == -1) {
            if (!running || errno == EINTR) {
                break;
            }
            perror("mq_send cpu");
            mq_close(mqd);
            return 1;
        }

        sleep(INTERVAL);
    }

    mq_close(mqd);
    return 0;
}