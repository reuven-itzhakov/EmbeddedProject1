#include <sysmon_ipc.h>

static volatile sig_atomic_t running = 1;

void handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from memory sensor and sends it to the memory message queue.\n");
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

    mqd_t mqd = mq_open(MEM_QUEUE_NAME, O_WRONLY);
    if (mqd == (mqd_t)-1) {
        perror("mq_open mem");
        return 1;
    }

    while (running) {
        FILE *file = fopen("/proc/meminfo", "r");
        if (file == NULL) {
            perror("Error opening /proc/meminfo");
            mq_close(mqd);
            return 1;
        }

        MemData mem_data = {0};
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            if (strncmp(buffer, "MemTotal:", 9) == 0) {
                mem_data.mem_total = strtol(buffer + 9, NULL, 10);
            } else if (strncmp(buffer, "MemFree:", 8) == 0) {
                mem_data.mem_free = strtol(buffer + 8, NULL, 10);
            } else if (strncmp(buffer, "MemAvailable:", 13) == 0) {
                mem_data.mem_avail = strtol(buffer + 13, NULL, 10);
            } else if (strncmp(buffer, "SwapTotal:", 10) == 0) {
                mem_data.swap_total = strtol(buffer + 10, NULL, 10);
            } else if (strncmp(buffer, "SwapFree:", 9) == 0) {
                mem_data.swap_free = strtol(buffer + 9, NULL, 10);
            }
        }
        fclose(file);

        if (mq_send(mqd, (const char *)&mem_data, sizeof(MemData), 1) == -1) {
            if (!running || errno == EINTR) {
                break;
            }
            perror("mq_send mem");
            mq_close(mqd);
            return 1;
        }

        sleep(INTERVAL);
    }

    mq_close(mqd);
    return 0;
}