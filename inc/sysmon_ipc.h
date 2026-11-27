#ifndef SYSMON_IPC_H
#define SYSMON_IPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <mqueue.h>

#define INTERVAL 1

#define MEM_QUEUE_NAME  "/sysmon_mem_mq"
#define CPU_QUEUE_NAME  "/sysmon_cpu_mq"
#define PROC_QUEUE_NAME "/sysmon_proc_mq"

#define MAX_MESSAGES 10

typedef struct {
    long mem_total;
    long mem_free;
    long mem_avail;
    long swap_total;
    long swap_free;
} MemData;

typedef struct {
    double load1;
    double load5;
    double load15;
} CpuData;

typedef struct {
    int total_procs;
} ProcData;

#endif /* SYSMON_IPC_H */