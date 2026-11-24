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
#include <poll.h>

#define CPU_FIFO_PATH  "/tmp/sysmon_cpu.fifo"
#define PROC_FIFO_PATH "/tmp/sysmon_proc.fifo"
#define INTERVAL 1

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
