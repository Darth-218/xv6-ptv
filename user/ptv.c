#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/pinfo.h"

#define NPROC 64
#define MAX_DEPTH 32

static const char *state_str[] = {
    [0] "UNUSED",
    [1] "USED",
    [2] "SLEEPING",
    [3] "RUNNABLE",
    [4] "RUNNING",
    [5] "ZOMBIE",
};

void print_process(struct pinfo *p, int depth) {
    for (int i = 0; i < depth; i++) {
        if (i == depth - 1)
            printf("  └── ");
        else
            printf("      ");
    }
    
    if (p->state == ZOMBIE)
        printf("%s(%d,ZOMBIE)\n", p->name, p->pid);
    else if (p->state == RUNNING)
        printf("%s(%d,RUN)\n", p->name, p->pid);
    else if (p->state == SLEEPING)
        printf("%s(%d,SLEEP)\n", p->name, p->pid);
    else if (p->state == RUNNABLE)
        printf("%s(%d,RDY)\n", p->name, p->pid);
    else
        printf("%s(%d)\n", p->name, p->pid);
}

void dfs(int parent_pid, int depth, struct pinfo *procs, int n, int *count) {
    for (int i = 0; i < n; i++) {
        if (procs[i].ppid == parent_pid) {
            (*count)++;
            print_process(&procs[i], depth);
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
    if (init->state == ZOMBIE)
        printf(",ZOMBIE");
    else if (init->state == RUNNING)
        printf(",RUN");
    else if (init->state == SLEEPING)
        printf(",SLEEP");
    else if (init->state == RUNNABLE)
        printf(",RDY");
    printf(")\n");
    total_count = 1;
    
    dfs(1, 1, procs, n, &total_count);
    
    printf("Total: %d processes\n", total_count);
    exit(0);
}