#include "sysmon_ipc.h"

static volatile sig_atomic_t running = 1;

void handler(int sig) {
    running = 0;
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

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [interval_sec]\n", argv[0]);
        printf("This program reads data from CPU sensor and writes it to the CPU FIFO.\n");
        return 0;
    }

    int interval = (argc > 1) ? parse_interval(argv[1], CPU_INTERVAL) : CPU_INTERVAL;

    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting signal handler");
        return 1;
    }

    if (mkfifo(CPU_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating CPU_FIFO_PATH");
        return 1;
    }

    int fd = open(CPU_FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        if (!running && errno == EINTR) {
            return 0;
        }
        perror("Error opening CPU_FIFO_PATH");
        return 1;
    }

    while (running) {
        FILE *file = fopen("/proc/loadavg", "r");
        if (file == NULL) {
            perror("Error opening /proc/loadavg");
            close(fd);
            return 1;
        }

        CpuData cpu_data = {0};
        int parsed = fscanf(file, "%lf %lf %lf", &cpu_data.load1, &cpu_data.load5, &cpu_data.load15);
        fclose(file);

        if (parsed != 3) {
            fprintf(stderr, "Error reading load averages\n");
            sleep((unsigned int)interval);
            continue;
        }

        if (write(fd, &cpu_data, sizeof(CpuData)) != sizeof(CpuData)) {
            if (!running || errno == EINTR || errno == EPIPE) {
                break;
            }
            perror("Error writing to CPU_FIFO_PATH");
            close(fd);
            return 1;
        }

        sleep((unsigned int)interval);
    }

    close(fd);
    return 0;
}