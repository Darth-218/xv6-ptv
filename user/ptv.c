// user/ptv.c - Enhanced Process Tree Visualizer with Colors
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/pinfo.h"

#define NPROC 64
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
    
    // Choose color based on state
    const char *color = COLOR_RESET;
    const char *state_tag = "";
    
    if (p->state == ZOMBIE) {
        color = COLOR_RED;
        state_tag = ",ZOMBIE";
    } else if (p->state == RUNNING) {
        color = COLOR_GREEN;
        state_tag = ",RUN";
    } else if (p->state == SLEEPING) {
        color = COLOR_BLUE;
        state_tag = ",SLEEP";
    } else if (p->state == RUNNABLE) {
        color = COLOR_YELLOW;
        state_tag = ",RDY";
    } else if (p->state == USED) {
        color = COLOR_CYAN;
        state_tag = "";
    } else {
        color = COLOR_RESET;
        state_tag = "";
    }
    
    // Make root bold
    if (is_root) {
        printf("%s%s%s(%d%s)%s\n", 
               COLOR_BOLD, color, p->name, p->pid, state_tag, COLOR_RESET);
    } else {
        printf("%s%s(%d%s)%s\n", 
               color, p->name, p->pid, state_tag, COLOR_RESET);
    }
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
    
    // Fetch process info from kernel
    n = getprocs(procs, NPROC);
    if (n < 0) {
        printf("ptv: getprocs failed\n");
        exit(1);
    }
    
    if (n == 0) {
        printf("No processes found\n");
        exit(0);
    }
    
    // Find init (PID 1) as root
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
    
    // Print root (bold + color based on state)
    printf("\n");  // Blank line for better visibility
    
    const char *color = COLOR_RESET;
    const char *state_tag = "";
    
    if (init->state == ZOMBIE) {
        color = COLOR_RED;
        state_tag = ",ZOMBIE";
    } else if (init->state == RUNNING) {
        color = COLOR_GREEN;
        state_tag = ",RUN";
    } else if (init->state == SLEEPING) {
        color = COLOR_BLUE;
        state_tag = ",SLEEP";
    } else if (init->state == RUNNABLE) {
        color = COLOR_YELLOW;
        state_tag = ",RDY";
    }
    
    printf("%s%s%s(%d%s)%s\n", 
           COLOR_BOLD, color, init->name, init->pid, state_tag, COLOR_RESET);
    total_count = 1;
    
    // Print children with DFS
    dfs(1, 1, procs, n, &total_count);
    
    // Footer with color
    printf("\n\033[37mTotal: %d processes\033[0m\n", total_count);  // White/gray
    
    // Optional: Print legend
    printf("\n\033[90mLegend:\033[0m "
           "\033[32m● RUN\033[0m "
           "\033[34m● SLEEP\033[0m "
           "\033[33m● RDY\033[0m "
           "\033[31m● ZOMBIE\033[0m "
           "\033[36m● NORMAL\033[0m\n");
    
    exit(0);
}