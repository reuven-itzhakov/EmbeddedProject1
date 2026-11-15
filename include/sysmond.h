#ifndef SYSMOND_H
#define SYSMOND_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pthread.h>

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

extern SystemState* state;

void *mem_sensor(void *arg);
void *cpu_sensor(void *arg);
void *proc_sensor(void *arg);
void *alert_handler(void *arg);

#endif