// user/ptv.c - Enhanced Process Tree Visualizer with Colors
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/pinfo.h"

#define MAX_DEPTH 32

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"     // Zombie, Unused
#define COLOR_GREEN   "\033[32m"     // Running
#define COLOR_YELLOW  "\033[33m"     // Runnable
#define COLOR_BLUE    "\033[34m"     // Sleeping
#define COLOR_CYAN    "\033[36m"     // Normal/USED
#define COLOR_BOLD    "\033[1m"      // Bold for root (init)

// Print a single process with state-aware formatting and colors
void print_process(struct pinfo *p, int depth, int is_root) {
    // Indentation
    for (int i = 0; i < depth; i++) {
        if (i == depth - 1)
            printf("  └── ");
        else
            printf("      ");
    }

    if (p->state == PINFO_ZOMBIE)
        printf("%s(%d,ZOMBIE)\n", p->name, p->pid);
    else if (p->state == PINFO_RUNNING)
        printf("%s(%d,RUN)\n", p->name, p->pid);
    else if (p->state == PINFO_SLEEPING)
        printf("%s(%d,SLEEP)\n", p->name, p->pid);
    else if (p->state == PINFO_RUNNABLE)
        printf("%s(%d,RDY)\n", p->name, p->pid);
    else
        printf("%s(%d)\n", p->name, p->pid);
}

// DFS using linear scan (handles large PIDs safely)
void dfs(int parent_pid, int depth, struct pinfo *procs, int n, int *count) {
    for (int i = 0; i < n; i++) {
        if (procs[i].ppid == parent_pid) {
            (*count)++;
            print_process(&procs[i], depth, 0);  // Not root
            dfs(procs[i].pid, depth + 1, procs, n, count);
        }
    }
}

int main(void) {
    struct pinfo procs[NPROC];
    int n, total_count = 0;
    n = getprocs(procs, NPROC);
    if (n < 0) {
        printf("ptv: getprocs failed\n");
        exit(1);
    }

    if (n == 0) {
        printf("No processes found\n");
        exit(0);
    }
    struct pinfo *init = 0;
    for (int i = 0; i < n; i++) {
        if (procs[i].pid == 1) {
            init = &procs[i];
            break;
        }
    }

    if (!init) {
        printf("ptv: init process not found\n");
        exit(1);
    }

    printf("%s(%d", init->name, init->pid);
    if (init->state == PINFO_ZOMBIE)
        printf(",ZOMBIE");
    else if (init->state == PINFO_RUNNING)
        printf(",RUN");
    else if (init->state == PINFO_SLEEPING)
        printf(",SLEEP");
    else if (init->state == PINFO_RUNNABLE)
        printf(",RDY");
    printf(")\n");
    total_count = 1;

    dfs(1, 1, procs, n, &total_count);

    printf("Total: %d processes\n", total_count);
    exit(0);
}
