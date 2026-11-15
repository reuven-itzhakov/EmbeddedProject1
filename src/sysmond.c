#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>

typedef struct {
    // Memory Metrics
    long mem_total;
    long mem_free;
    long mem_avail;
    long swap_total;
    long swap_free;

    // CPU Load Metrics
    double load1;
    double load5;
    double load15;

    // Process Metrics
    int total_procs;

    // Synchronization
    pthread_mutex_t lock;
    pthread_cond_t alert_cond;
} SystemState;

SystemState* state;

void *mem_sensor(void *arg) {
    useconds_t interval = (useconds_t)arg;
    while(1) {
        FILE *file = fopen("/proc/meminfo", "r");
        if(file == NULL) {
            perror("Error opening /proc/meminfo");
            return 1;
        }
        
        char buffer[256];
        long m_total = 0, m_free = 0, m_avail = 0, s_total = 0, s_free = 0;

        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            if (strncmp(buffer, "MemTotal:", 9) == 0) {
                m_total = strtol(buffer + 9, NULL, 10);
            } 
            else if (strncmp(buffer, "MemFree:", 8) == 0) {
                m_free = strtol(buffer + 8, NULL, 10);
            } 
            else if (strncmp(buffer, "MemAvailable:", 13) == 0) {
                m_avail = strtol(buffer + 13, NULL, 10);
            } 
            else if (strncmp(buffer, "SwapTotal:", 10) == 0) {
                s_total = strtol(buffer + 10, NULL, 10);
            } 
            else if (strncmp(buffer, "SwapFree:", 9) == 0) {
                s_free = strtol(buffer + 9, NULL, 10);
            }
        }

        pthread_mutex_lock(&state->lock);
        state->mem_total  = m_total;
        state->mem_free   = m_free;
        state->mem_avail  = m_avail;
        state->swap_total = s_total;
        state->swap_free  = s_free;
        pthread_mutex_unlock(&state->lock);

        fclose(file);

        usleep(interval);
    }
    return NULL;
}

void *cpu_sensor(void *arg) {
    useconds_t interval = (useconds_t)arg;
    while(1) {
        double loadavg1, loadavg5, loadavg15;
    
        FILE *file = fopen("/proc/loadavg", "r");
        if(file == NULL) {
            perror("Error opening /proc/loadavg");
            return 1;
        }
        
        if(fscanf(file, "%lf %lf %lf", &loadavg1, &loadavg5, &loadavg15) == 3) {
            pthread_mutex_lock(&state->lock);
            state->load1 = loadavg1;
            state->load5 = loadavg5;
            state->load15 = loadavg15;
            pthread_mutex_unlock(&state->lock);
        } else {
            perror("Error reading load averages");
        }

        fclose(file);

        usleep(interval);
    }
    return NULL;
}

void *proc_sensor(void *arg) {
    useconds_t interval = (useconds_t)arg;
    while(1){
        DIR *dir = opendir("/proc");
        if (dir == NULL) {
            perror("Error opening /proc");
            return 1;
        }
    
        size_t total = 0;
        struct dirent *entry;

        while((entry = readdir(dir)) != NULL) {
            if (atoi(entry->d_name) > 0) {
                total++;
            }
        }
        
        pthread_mutex_lock(&state->lock);
        state->total_procs = total;
        pthread_mutex_unlock(&state->lock);

        closedir(dir);

        usleep(interval);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [interval]\n", argv[0]);
        printf("interval: Time in seconds between updates (default is 1 second)\n");
        printf("This program reads data from memory, CPU, and process sensors and displays it.\n");
        return 0;
    }

    useconds_t interval = 1000000;
    if(argc > 1){
        interval = atof(argv[1]) * 1000000;
    }

    mkdir("/tmp/sysmon", 0777);
    
    state = malloc(sizeof(SystemState));
    if(state == NULL) {
        perror("Error allocating memory for SystemState");
        return 1;
    }

    pthread_mutex_init(&state->lock, NULL);
    pthread_cond_init(&state->alert_cond, NULL);

    pthread_t tid_mem_sensor;
    if(pthread_create(&tid_mem_sensor, NULL, mem_sensor, (void*)interval) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    pthread_t tid_cpu_sensor;
    if(pthread_create(&tid_cpu_sensor, NULL, cpu_sensor, (void*)interval) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    pthread_t tid_proc_sensor;
    if(pthread_create(&tid_proc_sensor, NULL, proc_sensor, (void*)interval) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }

    while(1) {
        system("clear");

        pthread_mutex_lock(&state->lock);
        printf("MemTotal:        %zu kB\n", state->mem_total);
        printf("MemFree:         %zu kB\n", state->mem_free);
        printf("MemAvailable:    %zu kB\n", state->mem_avail);
        printf("SwapTotal:       %zu kB\n", state->swap_total);
        printf("SwapFree:        %zu kB\n", state->swap_free);
        printf("Load Average (1 min): %.2f\n", state->load1);
        printf("Load Average (5 min): %.2f\n", state->load5);
        printf("Load Average (15 min): %.2f\n", state->load15);
        printf("Total number of processes: %zu\n", state->total_procs);
        pthread_mutex_unlock(&state->lock);

        usleep(interval);
    }

    pthread_join(tid_mem_sensor, NULL);
    pthread_join(tid_cpu_sensor, NULL);
    pthread_join(tid_proc_sensor, NULL);

    return 0;
}