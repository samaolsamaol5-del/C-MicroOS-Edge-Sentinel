#ifndef OS_PULSE_CORE_H
#define OS_PULSE_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    int pid;
    char name[64];
    double mem_mb;
} ProcessInfo;

typedef struct {
    double cpu_usage;
    unsigned long long total_ram_mb;
    unsigned long long used_ram_mb;
    unsigned long long free_ram_mb;
    unsigned long long total_disk_gb;
    unsigned long long free_disk_gb;
    unsigned long uptime_seconds;
    ProcessInfo top_procs[5];
    int proc_count;
} SystemMetrics;

SystemMetrics get_system_metrics(void);
void print_dashboard(SystemMetrics metrics);
void print_json(SystemMetrics metrics);
void format_json(SystemMetrics metrics, char *buffer, size_t size);
void sleep_ms(int milliseconds);
void start_web_server(int port);

#endif
