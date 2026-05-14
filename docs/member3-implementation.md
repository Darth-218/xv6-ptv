# Member 3 — User-Space Visualizer Developer

## Phase 4: User-Space Syscall Stub

### `user/usys.pl:45` — Add ecall trampoline entry

Add at end of file (after line 44 `entry("uptime");`):

```perl
entry("getprocs");
```

This generates the assembly trampoline in `user/usys.S`:
```asm
.global getprocs
getprocs:
 li a7, SYS_getprocs
 ecall
 ret
```

### `user/user.h:2,26` — Declare the syscall

Add include at top (after line 1):
```c
#include "kernel/pinfo.h"
```

Add declaration with existing syscalls (after line 26 `int uptime(void);`):
```c
int getprocs(struct pinfo*, int);
```

## Phase 5: Visualizer Program (`user/pstree.c`)

### Algorithm

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/pinfo.h"

#define MAX_CHILDREN 64

struct child_list {
    int pids[MAX_CHILDREN];
    int count;
};

struct child_list children[NPROC];  // indexed by ppid

void dfs(int pid, int depth, struct pinfo *procs, int n) {
    // print current process with indentation
    for (int i = 0; i < depth; i++)
        printf("  ");
    if (depth > 0)
        printf("\\342\\224\\224\\342\\224\\200 ");  // "└── " in octal escapes
    
    // find the pinfo entry for this pid
    for (int i = 0; i < n; i++) {
        if (procs[i].pid == pid) {
            if (procs[i].state == 5)  // ZOMBIE
                printf("%s(%d,ZOMBIE)\n", procs[i].name, pid);
            else
                printf("%s(%d)\n", procs[i].name, pid);
            break;
        }
    }
    
    // recurse into children
    for (int i = 0; i < children[pid].count; i++)
        dfs(children[pid].pids[i], depth + 1, procs, n);
}

int main(void) {
    struct pinfo procs[NPROC];
    int n = getprocs(procs, NPROC);
    if (n < 0) {
        printf("getprocs failed\n");
        exit(1);
    }
    
    // build children lists
    for (int i = 0; i < n; i++) {
        int ppid = procs[i].ppid;
        if (ppid >= 0 && ppid < NPROC)
            children[ppid].pids[children[ppid].count++] = procs[i].pid;
    }
    
    // find roots and DFS
    int visited = 0;
    for (int i = 0; i < n; i++) {
        if (procs[i].ppid == -1) {
            dfs(procs[i].pid, 0, procs, n);
            visited++;
        }
    }
    
    // fallback: if no roots found with ppid==-1, start from init
    if (visited == 0)
        dfs(1, 0, procs, n);
    
    exit(0);
}
```

### `Makefile:147` — Add to UPROGS

```makefile
$U/_pstree\
```

Add in alphabetical position or at end of the `UPROGS=\` list (before the blank line).

### Edge cases

| Case | Behavior |
|------|----------|
| **Orphan processes** | Handled by root detection (`ppid == -1`); each root is printed separately |
| **Zombie processes** | Detected by `state == 5`; append `,ZOMBIE` to the label |
| **Empty process table** | `getprocs` returns 0; pstree prints nothing and exits cleanly |
| **Large trees** | `MAX_CHILDREN = 64` matches NPROC limit; each parent can have up to 64 children |
| **PID 1 not found** | Fallback prints from PID 1 regardless; handles edge case where init is temporarily UNUSED |
| **Buffer overflow** | `children[]` indexed by PID; PIDs can exceed NPROC-1; guard with `ppid >= 0 && ppid < NPROC` |

### Expected output

```
init(1)
  └── sh(2)
      └── pstree(3)
```
