#include <sysmon_ipc.h>

static volatile sig_atomic_t running = 1;

void handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from process sensor and sends it to the process message queue.\n");
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

    mqd_t mqd = mq_open(PROC_QUEUE_NAME, O_WRONLY);
    if (mqd == (mqd_t)-1) {
        perror("mq_open proc");
        return 1;
    }

    while (running) {
        DIR *dir = opendir("/proc");
        if (dir == NULL) {
            perror("Error opening /proc");
            mq_close(mqd);
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

        if (mq_send(mqd, (const char *)&proc_data, sizeof(ProcData), 1) == -1) {
            if (!running || errno == EINTR) {
                break;
            }
            perror("mq_send proc");
            mq_close(mqd);
            return 1;
        }

        sleep(INTERVAL);
    }

    mq_close(mqd);
    return 0;
}