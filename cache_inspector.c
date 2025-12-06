#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>

#define MAX_PIDS 4096

typedef struct {
    int pid;
    char name[64];
    double cpu;
    long mem_kb;
    double cache;
    double score;
} ProcInfo;

// ─────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────
void get_process_name(int pid, char *name_buf, size_t size) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(name_buf, size, f))
            name_buf[strcspn(name_buf, "\n")] = '\0';
        else strncpy(name_buf, "unknown", size);
        fclose(f);
    } else strncpy(name_buf, "unknown", size);
}

long get_memory_usage_kb(int pid) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/statm", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long t, rss;
    if (fscanf(f, "%ld %ld", &t, &rss) != 2) { fclose(f); return -1; }
    fclose(f);
    return rss * (sysconf(_SC_PAGESIZE) / 1024);
}

unsigned long long get_cpu_time(int pid) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long long utime, stime;
    char buf[1024];
    for (int i = 0; i < 13; i++) fscanf(f, "%s", buf);
    fscanf(f, "%llu %llu", &utime, &stime);
    fclose(f);
    return utime + stime;
}

double check_file_cache(const char *filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) return -1.0;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return -1.0; }
    if (st.st_size == 0) { close(fd); return 0.0; }

    long page_size = sysconf(_SC_PAGESIZE);
    size_t pages = (st.st_size + page_size - 1) / page_size;

    void *addr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) { close(fd); return -1.0; }

    unsigned char *vec = calloc(1, pages);
    if (!vec) { munmap(addr, st.st_size); close(fd); return -1.0; }

    if (mincore(addr, st.st_size, vec) != 0) {
        free(vec); munmap(addr, st.st_size); close(fd); return -1.0;
    }

    size_t cached = 0;
    for (size_t i = 0; i < pages; i++) if (vec[i] & 1) cached++;
    double pct = (cached * 100.0) / pages;

    free(vec);
    munmap(addr, st.st_size);
    close(fd);
    return pct;
}

// ─────────────────────────────────────────────
// Sort by combined impact score
// ─────────────────────────────────────────────
int cmp_score(const void *a, const void *b) {
    double sa = ((ProcInfo*)a)->score;
    double sb = ((ProcInfo*)b)->score;
    return (sb > sa) - (sb < sa); // descending
}

// ─────────────────────────────────────────────
// Gather processes and compute score
// ─────────────────────────────────────────────
int gather_processes(ProcInfo infos[], unsigned long long prev_cpu[]) {
    DIR *proc = opendir("/proc");
    if (!proc) return 0;

    struct dirent *e;
    int count = 0;

    while ((e = readdir(proc)) != NULL) {
        if (!isdigit(e->d_name[0])) continue;
        int pid = atoi(e->d_name);
        if (pid <= 0 || count >= MAX_PIDS) continue;

        ProcInfo *p = &infos[count];
        p->pid = pid;
        get_process_name(pid, p->name, sizeof(p->name));
        p->mem_kb = get_memory_usage_kb(pid);

        unsigned long long cpu_now = get_cpu_time(pid);
        double cpu_pct = 0.0;
        if (prev_cpu[pid] != 0) {
            unsigned long long diff = cpu_now - prev_cpu[pid];
            cpu_pct = (diff * 100.0) / sysconf(_SC_CLK_TCK);
        }
        prev_cpu[pid] = cpu_now;
        p->cpu = cpu_pct;

        char exe_path[PATH_MAX], real_exe[PATH_MAX];
        snprintf(exe_path, sizeof(exe_path), "/proc/%d/exe", pid);
        ssize_t len = readlink(exe_path, real_exe, sizeof(real_exe) - 1);
        p->cache = (len != -1) ? check_file_cache(real_exe) : 0.0;

        // compute weighted impact score
        double mem_mb = p->mem_kb / 1024.0;
        p->score = (p->cpu * 0.6) + (mem_mb * 0.3) + (p->cache * 0.1);

        count++;
    }

    closedir(proc);
    return count;
}

// ─────────────────────────────────────────────
// Display ranked list
// ─────────────────────────────────────────────
void show_top(ProcInfo infos[], int count) {
    printf("%-6s %-22s %8s %12s %10s %10s\n",
           "PID", "Process", "CPU%", "Mem(KB)", "Cache%", "Score");
    printf("──────────────────────────────────────────────────────────────────────\n");

    for (int i = 0; i < count && i < 30; i++) {
        printf("%-6d %-22s %8.2f %12ld %10.2f %10.2f\n",
               infos[i].pid, infos[i].name,
               infos[i].cpu, infos[i].mem_kb,
               infos[i].cache, infos[i].score);
    }
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────
int main(void) {
    unsigned long long prev_cpu[65536] = {0};

    while (1) {
        system("clear");
        printf("========== Filesystem Cache Inspector ==========\n");
        printf("Mode: combined resource impact (refresh 1s)\n");
        printf("──────────────────────────────────────────────────────────────────────\n");

        ProcInfo infos[MAX_PIDS];
        int count = gather_processes(infos, prev_cpu);

        double total_cpu = 0.0;
        long total_mem = 0;
        double avg_cache = 0.0;
        for (int i = 0; i < count; i++) {
            total_cpu += infos[i].cpu;
            total_mem += infos[i].mem_kb;
            avg_cache += infos[i].cache;
        }
        if (count > 0) avg_cache /= count;

        printf("\nSystem-Wide Summary\n");
        printf("────────────────────────────────────────────\n");
        printf("Total CPU usage:    %8.2f %%\n", total_cpu);
        printf("Total Memory usage: %8.2f GB\n", total_mem / 1024.0 / 1024.0);
        printf("Avg Cache usage:    %8.2f %%\n", avg_cache);
        printf("Processes counted:  %8d\n", count);
        printf("────────────────────────────────────────────\n\n");

        qsort(infos, count, sizeof(ProcInfo), cmp_score);
        show_top(infos, count);

        fflush(stdout);
        sleep(1);
    }
    return 0;
}
