#include "sysmon_ipc.h"

volatile int fd;

void handler(int sig) {
    close(fd);
    unlink(PROC_FIFO_PATH);
    exit(0);
}

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        printf("This program reads data from process sensor and displays it.\n");
        return 0;
    }
    
    if(mkfifo(PROC_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating PROC_FIFO_PATH");
        return 1;
    }

    
    fd = open(PROC_FIFO_PATH, O_WRONLY);
    if(fd == -1) {
        perror("Error opening PROC_FIFO_PATH");
        return 1;
    }
    
    struct sigaction sa;
    sa.sa_handler = &handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Error setting signal handler");
        return 1;
    }
    
    while(1){
        DIR *dir = opendir("/proc");
        if (dir == NULL) {
            perror("Error opening /proc");
            return 1;
        }
        ProcData proc_data;

        size_t total = 0;
        struct dirent *entry;

        while((entry = readdir(dir)) != NULL) {
            if (atoi(entry->d_name) > 0) {
                total++;
            }
        }
        
        proc_data.total_procs = total;
        
        if(write(fd, &proc_data, sizeof(ProcData)) == -1) {
            perror("Error writing to PROC_FIFO_PATH");
            return 1;
        }
    
        closedir(dir);

        sleep(INTERVAL);
    }
        
    return 0;
}