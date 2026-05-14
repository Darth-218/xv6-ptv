# Member 1 — Kernel Process Data Exposure

## Phase 1: Shared Data Structure (`kernel/pinfo.h`)

Created `kernel/pinfo.h` defining `struct pinfo`, the shared data structure
used to exchange process metadata between the kernel and user space.

```c
struct pinfo {
    int pid;
    int ppid;   // parent PID, -1 if no parent (e.g. init)
    int state;  // enum procstate value (0=UNUSED .. 5=ZOMBIE)
    char name[16];
};
```

Fields:
- **pid**: Process ID (from `struct proc->pid`)
- **ppid**: Parent process ID (from `struct proc->parent->pid`). Set to `-1` when
  the process has no parent (init/PID 1, or orphan whose parent was reaped).
- **state**: Integer matching `enum procstate` in `kernel/proc.h`:
  `0=UNUSED`, `1=USED`, `2=SLEEPING`, `3=RUNNABLE`, `4=RUNNING`, `5=ZOMBIE`.
  `UNUSED` entries are excluded from results.
- **name**: Null-terminated process name (max 16 bytes including NUL).

This header is included by both kernel (`kernel/sysproc.c`) and user-space
programs (`user/pstree.c`, `user/user.h`).

## Phase 3: Kernel Syscall Handler (`kernel/sysproc.c`)

Implemented `sys_getprocs()` in `kernel/sysproc.c`.

### Signature

```c
uint64 sys_getprocs(void)
```

### Arguments (from user space via registers a0, a1)

| Arg | Type | Description |
|-----|------|-------------|
| a0  | uint64 | User-space address of `struct pinfo` buffer |
| a1  | int    | Maximum number of entries to copy (`max_count`) |

### Locking Protocol

Follows xv6 conventions (see `kwait`/`reparent` in `kernel/proc.c`):

1. **`acquire(&wait_lock)`** — Protects `p->parent` pointer from concurrent
   modification (e.g. `reparent()` during `kexit()`).
2. For each process `p` in `proc[NPROC]`:
   - **`acquire(&p->lock)`** — Protects per-process fields (`state`, `pid`, `name`).
   - Read fields while both `wait_lock` and `p->lock` are held.
   - Read `p->parent->pid` only if `p->parent != 0`.
   - Release `p->lock` after reading.
3. **`release(&wait_lock)`** after loop completes.

### Algorithm

```
acquire(wait_lock)
count = 0
for each p in proc[NPROC]:
    acquire(p->lock)
    if p->state == UNUSED:
        release(p->lock)
        continue
    pi.pid   = p->pid
    pi.state = p->state
    safestrcpy(pi.name, p->name, sizeof(pi.name))
    pi.ppid  = (p->parent != 0) ? p->parent->pid : -1
    release(p->lock)
    if copyout(curr_pagetable, buf + count*sizeof(pinfo), &pi, sizeof(pi)) < 0:
        release(wait_lock)
        return -1
    count++
    if count >= max_count:
        break
release(wait_lock)
return count
```

### Edge Cases Handled

| Case | Behavior |
|------|----------|
| **Zombie** | Included; state=5 is reported so visualizer can mark it (e.g. `sh(2,ZOMBIE)`) |
| **Orphan** | `ppid = -1` if `p->parent` is NULL |
| **Init (PID 1)** | `ppid = -1` (no parent) |
| **Buffer full** | Stop after `max_count` entries; return partial count |
| **copyout failure** | Release all locks, return `-1` |
| **Concurrent fork/exit** | `wait_lock` + `p->lock` ensure consistency |

### Syscall Contract

- **Input**: `(uint64 buf_addr, int max_count)` via `argaddr()` and `argint()`
- **Access**: `acquire(wait_lock)`, then per-entry `acquire(p->lock)`
- **Output**: Count of processes copied to `buf`, or `-1` on `copyout()` failure
- **Guarantee**: Each `struct pinfo` has valid `pid`, `ppid` (-1 if no parent),
  `state` (0-5), `name` (null-terminated, max 16 bytes)
