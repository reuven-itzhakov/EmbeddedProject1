#include "sysmon_ipc.h"

static volatile sig_atomic_t running = 1;

void handler(int sig) {
    running = 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from process sensor and writes it to the process FIFO.\n");
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

    if (mkfifo(PROC_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating PROC_FIFO_PATH");
        return 1;
    }

    int fd = open(PROC_FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        if (!running && errno == EINTR) {
            return 0;
        }
        perror("Error opening PROC_FIFO_PATH");
        return 1;
    }

    while (running) {
        DIR *dir = opendir("/proc");
        if (dir == NULL) {
            perror("Error opening /proc");
            close(fd);
            return 1;
        }

        int total = 0;
        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL) {
            if (atoi(entry->d_name) > 0) {
                total++;
            }
        }
        closedir(dir);

        ProcData proc_data = {0};
        proc_data.total_procs = total;

        if (write(fd, &proc_data, sizeof(ProcData)) != sizeof(ProcData)) {
            if (!running || errno == EINTR || errno == EPIPE) {
                break;
            }
            perror("Error writing to PROC_FIFO_PATH");
            close(fd);
            return 1;
        }

        sleep(INTERVAL);
    }

    close(fd);
    return 0;
}
