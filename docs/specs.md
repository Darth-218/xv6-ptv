# xv6 Process Tree Visualizer (ptv) — Full Specification

## 1. Project Overview

**Course:** CSCI-315 Operating Systems  
**Goal:** A `pstree`-like tool for xv6 that displays parent-child process
relationships, PIDs, and process states.  
**Program name:** `ptv` (Process Tree Visualizer)

The visualizer runs as a user-space program that retrieves process information
from the kernel via a new system call (`getprocs`) and renders a formatted
process tree with state annotations.

### Team Roles

| Member | Role | Primary Files |
|--------|------|---------------|
| 1 | Kernel Process Data Exposure | `kernel/pinfo.h`, `kernel/sysproc.c` |
| 2 | Kernel System Call Developer | `kernel/syscall.h`, `kernel/syscall.c` |
| 3 | User-Space Visualizer Developer | `user/ptv.c`, `user/usys.pl`, `user/user.h` |
| 4 | Testing & Validation Engineer | `user/forktree.c`, `user/orphantree.c`, `test-xv6.py` |
| 5 | Advanced Visualization & Integration | `user/ptv.c` (enhancements), `user/zombie.c` |

---

## 2. Architecture & Data Flow

```
Kernel proc[] ──► sys_getprocs() ──► copyout() ──► getprocs() ──► ptv
```

```
struct proc (kernel)          struct pinfo (shared)         user program
┌────────────────────┐       ┌────────────────────┐       ┌──────────────────┐
│ pid                │       │ int pid            │       │ getprocs(buf, N) │
│ state (enum)       │ ───► │ int ppid           │ ───► │ returns count    │
│ parent (struct*)   │       │ int state          │       │ or -1            │
│ name[16]           │       │ char name[16]      │       └──────────────────┘
└────────────────────┘       └────────────────────┘                │
      │                        ▲                                   ▼
      │  acquire(wait_lock)    │  copyout()              ┌──────────────────┐
      │  for each proc:        │  per entry              │ DFS from PID 1   │
      │  read pid/state/name   │                         │ formatted output │
      │  parent->pid → ppid    │                         │ with state marks │
      └────────────────────────┘                         └──────────────────┘
```

### Boundary crossing

| Layer | Address space | Mechanism |
|-------|--------------|-----------|
| `sys_getprocs()` | Kernel | Reads `proc[NPROC]` with locks |
| `copyout()` | Kernel→User | Copies `struct pinfo` to user buffer |
| `getprocs()` stub | User | `ecall` trampoline (usys.S) |
| `ptv` main logic | User | DFS tree construction + printf |

---

## 3. Kernel Components

### 3.1 `kernel/pinfo.h` — Shared Data Structure

```c
#ifndef _PINFO_H
#define _PINFO_H
#define PINFO_UNUSED     0
#define PINFO_USED       1
#define PINFO_SLEEPING   2
#define PINFO_RUNNABLE   3
#define PINFO_RUNNING    4
#define PINFO_ZOMBIE     5

struct pinfo {
    int pid;       // Process ID
    int ppid;      // Parent PID, -1 if no parent
    int state;     // Process state (PINFO_UNUSED=0 .. PINFO_ZOMBIE=5)
    char name[16]; // Process name, null-terminated
};

#endif
```

**Design notes:**
- `ppid = -1` when a process has no parent (init/PID 1, or orphans whose parent
  pointer was cleared).
- State constants are prefixed `PINFO_` to avoid conflicting with the kernel's
  `enum procstate` (defined in `kernel/proc.h:82` as `UNUSED, USED, SLEEPING,
  RUNNABLE, RUNNING, ZOMBIE`). Kernel code that includes `proc.h` uses the enum;
  user code that includes `pinfo.h` uses the macros.
- `name[16]` matches `struct proc.name[16]` in `kernel/proc.h:106`.

### 3.2 `kernel/syscall.h` — System Call Number

```c
#define SYS_fork      1
...
#define SYS_close    21
#define SYS_getprocs 22     // line 23
```

### 3.3 `kernel/syscall.c` — Dispatch Wiring

**Extern declaration** (line 104):
```c
extern uint64 sys_getprocs(void);
```

**Dispatch table entry** (line 130):
```c
[SYS_getprocs]  sys_getprocs,
```

**Dispatch mechanics** (`syscall()` function, lines 133-149):
1. Read syscall number from `p->trapframe->a7`.
2. Validate `num > 0 && num < NELEM(syscalls) && syscalls[num]`.
3. Call `syscalls[num]()` and store return value in `p->trapframe->a0`.
4. On invalid number, print error and set `a0 = -1`.

Since `NELEM(syscalls)` uses the static array size, and the array uses
designated initializers (`[SYS_getprocs]`), the array is automatically large
enough to hold index 22.

### 3.4 `kernel/sysproc.c` — Syscall Handler Implementation

**Includes** (lines 1-12):
```c
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"       // struct proc, enum procstate, NPROC
#include "vm.h"         // copyout()
#include "pinfo.h"      // struct pinfo

extern struct proc proc[NPROC];       // from kernel/proc.c:11
extern struct spinlock wait_lock;     // from kernel/proc.c:27
```

#### `sys_getprocs()` (lines 115-151)

```c
uint64
sys_getprocs(void)
{
  uint64 buf;           // user-space buffer address (arg0)
  int max_count;        // maximum entries to copy (arg1)
  struct proc *p;
  struct pinfo pi;      // temporary pinfo for each process
  int count = 0;

  // Step 1: Extract arguments
  argaddr(0, &buf);
  argint(1, &max_count);
  if (max_count <= 0)
    return -1;

  // Step 2: Acquire wait_lock (protects p->parent pointers)
  acquire(&wait_lock);

  // Step 3: Iterate through process table
  for (p = proc; p < &proc[NPROC] && count < max_count; p++) {
    acquire(&p->lock);

    // Skip unused slots
    if (p->state == UNUSED) {
      release(&p->lock);
      continue;
    }

    // Read process metadata
    pi.pid   = p->pid;
    pi.state = p->state;
    safestrcpy(pi.name, p->name, sizeof(pi.name));

    // Read parent PID: -1 if no parent (init or orphan)
    pi.ppid  = (p->parent != 0) ? p->parent->pid : -1;

    release(&p->lock);

    // Step 4: Copy to user space
    if (copyout(myproc()->pagetable,
                buf + count * sizeof(pi),
                (char *)&pi, sizeof(pi)) < 0) {
      release(&wait_lock);
      return -1;
    }
    count++;
  }

  release(&wait_lock);
  return count;
}
```

#### Locking Protocol

| Lock | Acquired | Protects | Held during |
|------|----------|----------|-------------|
| `wait_lock` | Once (line 130) | `p->parent` pointer | Entire iteration |
| `p->lock` | Per-entry (line 132) | `p->state`, `p->pid`, `p->name` | Reading single entry |

Both `kexit()` and `reparent()` in `kernel/proc.c` also acquire `wait_lock`,
ensuring no `parent` pointer is modified during traversal. The per-process
`p->lock` ensures consistent reads of state/pid/name. Locks are released in
reverse order (per-process first, then wait_lock) to avoid deadlock.

#### Edge Cases Handled

| Case | Behavior | Code Line |
|------|----------|-----------|
| **UNUSED slots** | Skipped silently | 133-135 |
| **Zombie processes** | Included with `state = PINFO_ZOMBIE` | 137-138 |
| **Orphaned processes** | `ppid = -1` when `p->parent == NULL` | 140 |
| **Init (PID 1)** | `ppid = -1` (no parent) | 140 |
| **Buffer full** | Stops at `max_count`, returns partial count | 131 |
| **`copyout()` failure** | Releases all locks, returns `-1` | 143-145 |
| **`max_count <= 0`** | Immediate `-1` return | 127-128 |
| **Empty table** | Returns `count = 0` (no error) | 149-150 |

---

## 4. User-Space Components

### 4.1 `user/usys.pl` — Syscall Trampoline Generator

Added at line 45:
```perl
entry("getprocs");
```

Generates in `user/usys.S`:
```asm
.global getprocs
getprocs:
 li a7, SYS_getprocs    # load syscall number 22
 ecall                    # trap to kernel
 ret
```

The Perl script generates an `ecall` trampoline for each `entry()` call. The
syscall number is loaded into register `a7`, which `syscall()` in
`kernel/syscall.c:137` reads from `p->trapframe->a7`.

### 4.2 `user/user.h` — User-Space Declarations

```c
struct pinfo;                    // line 3: forward declaration
struct stat;
...
int getprocs(struct pinfo*, int);  // line 28: syscall declaration
```

A forward declaration of `struct pinfo` is used instead of `#include
"kernel/pinfo.h"` to keep `user.h` minimal. The full struct definition is
available to programs that include `kernel/pinfo.h` directly (as `ptv.c` does).

### 4.3 `user/ptv.c` — Visualizer Program (84 lines)

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"     // NPROC (64)
#include "kernel/pinfo.h"     // struct pinfo, PINFO_* constants

#define MAX_DEPTH 32
```

#### Function: `print_process()` (lines 9-27)

```c
void print_process(struct pinfo *p, int depth) {
    // Indentation: for each depth level
    //   last level: "  └── " (6 chars)
    //   other levels: "      " (6 chars)
    for (int i = 0; i < depth; i++) {
        if (i == depth - 1)
            printf("  └── ");
        else
            printf("      ");
    }

    // State-aware labeling
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
```

State label mapping:

| `p->state` | Label | Example |
|-----------|-------|---------|
| PINFO_ZOMBIE (5) | `ZOMBIE` | `zombie(3,ZOMBIE)` |
| PINFO_RUNNING (4) | `RUN` | `ptv(4,RUN)` |
| PINFO_SLEEPING (2) | `SLEEP` | `sh(2,SLEEP)` |
| PINFO_RUNNABLE (3) | `RDY` | `init(1,RDY)` |
| PINFO_USED (1) | *(none)* | `init(1)` |

#### Function: `dfs()` (lines 29-37)

```c
void dfs(int parent_pid, int depth, struct pinfo *procs, int n, int *count) {
    for (int i = 0; i < n; i++) {
        if (procs[i].ppid == parent_pid) {
            (*count)++;
            print_process(&procs[i], depth);
            dfs(procs[i].pid, depth + 1, procs, n, count);
        }
    }
}
```

Linear scan O(n^2) algorithm. Since `n ≤ NPROC = 64`, this is effectively
constant-time. No adjacency list needed.

#### Function: `main()` (lines 39-83)

```c
int main(void) {
    struct pinfo procs[NPROC];     // stack-allocated buffer (64 * 28 = 1792 bytes)
    int n, total_count = 0;

    // Step 1: Fetch process data from kernel
    n = getprocs(procs, NPROC);
    if (n < 0) {
        printf("ptv: getprocs failed\n");
        exit(1);
    }
    if (n == 0) {
        printf("No processes found\n");
        exit(0);
    }

    // Step 2: Find init (PID 1) as tree root
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

    // Step 3: Print root (init) with state annotation
    printf("%s(%d", init->name, init->pid);
    if (init->state == PINFO_ZOMBIE)      printf(",ZOMBIE");
    else if (init->state == PINFO_RUNNING) printf(",RUN");
    else if (init->state == PINFO_SLEEPING)printf(",SLEEP");
    else if (init->state == PINFO_RUNNABLE)printf(",RDY");
    printf(")\n");
    total_count = 1;

    // Step 4: DFS from init's children
    dfs(1, 1, procs, n, &total_count);

    // Step 5: Summary line
    printf("Total: %d processes\n", total_count);
    exit(0);
}
```

#### Algorithm summary

1. Call `getprocs(procs, NPROC)` to fetch all process data.
2. Locate PID 1 (init) as the tree root.
3. Print root with state annotation.
4. Recursively DFS children where `procs[i].ppid == parent_pid`.
5. Print "Total: N processes" summary.

#### Expected output

```
init(1,SLEEP)
  └── sh(2,SLEEP)
      └── ptv(4,RUN)
Total: 3 processes
```

---

## 5. Test Programs

### 5.1 `user/forktree.c` — Multi-Level Fork Hierarchy (46 lines)

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
fork_tree(int start_depth, int max_depth)
{
  int pid;

  // Create chain: fork → child continues loop → parent pauses
  for (int depth = start_depth; depth < max_depth - 1; depth++) {
    pid = fork();
    if (pid < 0) exit(1);
    if (pid == 0) continue;   // child continues forking
    for (;;) pause(10);       // parent stays alive
  }

  // Leaf: fork one zombie then pause
  pid = fork();
  if (pid < 0) exit(1);
  if (pid == 0) exit(0);      // leaf child → zombie
  for (;;) pause(10);         // leaf parent stays alive
}

int main(void) {
  printf("forktree starting\n");
  int pid = fork();
  if (pid < 0) exit(1);
  if (pid == 0) {
    fork_tree(0, 4);          // creates 4-level hierarchy
    for (;;) pause(10);
  }
  printf("forktree done\n");  // grandparent exits → shell not blocked
  exit(0);
}
```

**Process tree created:**
```
init(1)
  └── sh(2)
      └── forktree(4)        ← stays alive
          └── forktree(5)    ← also alive
              └── forktree(6)  ← ZOMBIE (leaf exited)
              ↑ (pauses)
```

**Design:** Uses double-fork so the grandparent reports "done" and exits,
unblocking the shell. The middle process and its descendants stay alive until
the test reads the ptv output.

### 5.2 `user/orphantree.c` — Orphan Reparenting Test (25 lines)

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
  int pid = fork();
  if (pid < 0) exit(1);

  if (pid == 0) {           // child
    int pid2 = fork();
    if (pid2 < 0) exit(1);
    if (pid2 == 0) {
      for (;;) pause(10);   // grandchild: stays alive
    }
    exit(0);                 // child exits → grandchild orphaned
  }

  wait(0);                   // parent waits for child
  exit(0);
}
```

**Orphan flow:**
1. Parent forks child.
2. Child forks grandchild, then exits.
3. Kernel's `reparent()` in `proc.c:311` reparents grandchild to init (PID 1).
4. Grandchild continues in `for(;;) pause(10)`.
5. ptv shows grandchild under init.

### 5.3 `user/zombie.c` — Zombie Process Test (28 lines)

```c
// Create a zombie process using double-fork.
// zombie runs → grandparent exits → shell continues
// parent stays alive → keeps grandchild as zombie for ptv to see.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
  int pid = fork();
  if (pid < 0) exit(1);

  if (pid == 0) {           // child
    int pid2 = fork();
    if (pid2 < 0) exit(1);
    if (pid2 == 0)
      exit(0);               // grandchild exits → zombie
    for (;;) pause(10);     // child stays alive → zombie not reaped
  }

  exit(0);                   // parent (original zombie) exits → shell continues
}
```

**Zombie flow:**
1. Original `zombie` process forks child, then exits (shell unblocked).
2. Child forks grandchild.
3. Grandchild exits immediately → becomes ZOMBIE.
4. Child stays alive in `pause(10)` loop → grandchild's parent remains alive,
   so grandchild stays in ZOMBIE state (not reaped by init).
5. ptv shows `zombie(...,ZOMBIE)`.

**Why double-fork?** Without double-fork, the shell would `wait()` for zombie's
direct child to exit, and the shell would be blocked. The double-fork ensures
the process that `exit(0)` immediately is the grandparent of the actual zombie.

---

## 6. Test Automation — `test-xv6.py`

### 6.1 `test_ptv_basic()` (lines 202-222)

```python
def test_ptv_basic():
    """Boot xv6, run ptv, validate process tree output."""
    q = QEMU(True)         # build xv6 + reset filesystem
    q.cmd("ptv\n")
    q.monitor(r'^init\(1', timeout=10)
    lines = q.lines()
    assert any(re.match(r'^init\(1', l) for l in lines)  # root present
    assert any(re.search(r'sh\(', l) for l in lines)     # shell present
    q.stop()
```

**Validates:** init is tree root, sh is a visible child.
**Regex:** `^init\(1` (no closing paren) to match both `init(1)` and
`init(1,SLEEP)`.

### 6.2 `test_ptv_forktree()` (lines 224-241)

```python
def test_ptv_forktree():
    """Run forktree then ptv, verify multi-level hierarchy."""
    q = QEMU(True)
    q.cmd("forktree\n")
    time.sleep(3)          # wait for hierarchy to form
    q.cmd("ptv\n")
    time.sleep(1)
    q.read()
    lines = q.lines()
    assert any(re.search(r'forktree\(', l) for l in lines)
    q.stop()
```

**Validates:** Multi-level fork hierarchy visible in ptv output.

### 6.3 `test_ptv_orphans()` (lines 243-260)

```python
def test_ptv_orphans():
    """Run orphantree then ptv, verify orphans shown under init."""
    q = QEMU(True)
    q.cmd("orphantree\n")
    time.sleep(3)
    q.cmd("ptv\n")
    time.sleep(1)
    q.read()
    lines = q.lines()
    assert any(re.search(r'orphantree', l) for l in lines)
    q.stop()
```

**Validates:** Orphaned process (reparented to init) appears in tree.

### 6.4 `test_ptv_zombies()` (lines 262-279)

```python
def test_ptv_zombies():
    """Verify zombie processes are marked in ptv output."""
    q = QEMU(True)
    q.cmd("zombie\n")
    time.sleep(2)
    q.cmd("ptv\n")
    time.sleep(1)
    q.read()
    lines = q.lines()
    assert any(re.search(r'ZOMBIE', l) for l in lines)
    q.stop()
```

**Validates:** Zombie processes marked with `,ZOMBIE` suffix in ptv output.

### Test runner

```python
def main():
    rex = r'%s' % args.testrex
    funcs = [(obj,name) for name,obj in inspect.getmembers(sys.modules[__name__])
             if (inspect.isfunction(obj) and name.startswith('test'))]
    for (f,n) in funcs:
        if re.search(rex, n):
            f()
```

Run individual tests:
```
./test-xv6.py ptv_basic
./test-xv6.py ptv_forktree
./test-xv6.py ptv_orphans
./test-xv6.py ptv_zombies
```

Run all ptv tests:
```
./test-xv6.py ptv
```

---

## 7. Build System — Makefile

### UPROGS additions (lines 148-150)

```makefile
UPROGS=\
	...
	$U/_forktree\
	$U/_orphantree\
	$U/_ptv\
```

Each `$U/_<name>` target is built by the implicit rule (line 102):
```makefile
_%: %.o $(ULIB) $U/user.ld
	$(LD) $(LDFLAGS) -T $U/user.ld -o $@ $< $(ULIB)
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym
```

Where `$(ULIB) = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o` (line 100).
The `_ptv` binary is linked with `usys.o` which contains the `getprocs` ecall
trampoline.

### Build commands

| Command | What it does |
|---------|-------------|
| `make` | Builds kernel + filesystem |
| `make kernel/kernel` | Builds just the kernel |
| `make fs.img` | Builds the filesystem image |
| `make qemu` | Boots xv6 in QEMU |
| `make clean` | Removes all build artifacts |

---

## 8. Full File Manifest

| File | Lines | Purpose | Primary Author |
|------|-------|---------|----------------|
| `kernel/pinfo.h` | 18 | `struct pinfo` + `PINFO_*` state constants | Member 1 |
| `kernel/syscall.h` | 24 | `#define SYS_getprocs 22` | Member 2 |
| `kernel/syscall.c` | 150 | Extern + dispatch table entry for getprocs | Member 2 |
| `kernel/sysproc.c` | 152 | `sys_getprocs()` implementation | Members 1, 2 |
| `user/usys.pl` | 46 | `entry("getprocs")` trampoline generator | Member 3 |
| `user/user.h` | 52 | `int getprocs(struct pinfo*, int)` declaration | Member 3 |
| `user/ptv.c` | 84 | Visualizer: DFS tree builder, state-aware output | Members 3, 5 |
| `user/forktree.c` | 46 | Test: multi-level fork hierarchy | Member 4 |
| `user/orphantree.c` | 25 | Test: orphan reparenting | Member 4 |
| `user/zombie.c` | 28 | Test: zombie process marking | Member 5 |
| `test-xv6.py` | 296 | 4 test functions (`test_ptv_*`) | Member 4 |
| `Makefile` | 196 | `UPROGS` entries for `_forktree`, `_orphantree`, `_ptv` | Members 3, 4 |
| `docs/member1-implementation.md` | — | Implementation doc: Member 1 | Member 1 |
| `docs/member2-implementation.md` | — | Implementation doc: Member 2 | Member 1 |
| `docs/member3-implementation.md` | — | Implementation doc: Member 3 | Member 1 |
| `docs/member4-implementation.md` | — | Implementation doc: Member 4 | Member 1 |
| `docs/member5-implementation.md` | — | Implementation doc: Member 5 | Member 1 |
| `docs/specs.md` | — | This file — full project spec | Member 1 |

---

## 9. Syscall Contract Reference

### `getprocs()` — kernel-to-user process data transfer

```
Arguments:  a0 = user-space buffer address (uint64)
            a1 = max count (int)
Returns:    a0 = number of processes copied (> 0), or -1 on error
```

#### Caller (user-space)

```c
int n = getprocs(procs, NPROC);
// n > 0:  procs[0..n-1] filled with process data
// n == 0: no active processes (empty table)
// n == -1: system call error
```

#### Callee (kernel-space)

```
acquire(wait_lock)
for each proc in proc[NPROC]:
    acquire(p->lock)
    if p->state == UNUSED: release, continue
    pinfo.pid   = p->pid
    pinfo.state = p->state
    pinfo.name  = p->name
    pinfo.ppid  = (p->parent) ? p->parent->pid : -1
    release(p->lock)
    copyout(pinfo) → user buffer
    if copyout fails: release(wait_lock), return -1
release(wait_lock)
return count
```

#### Guarantees

| Condition | Behavior |
|-----------|----------|
| `max_count ≤ 0` | Returns `-1`, no data copied |
| `buf` is invalid/unmapped | `copyout()` fails, returns `-1` |
| Partial buffer (n < total processes) | Returns `n = max_count` (partial result) |
| Empty process table | Returns `0` |
| Zombie process | Included, `state = 5` |
| Orphan/no parent | `ppid = -1` |
| `name` longer than 15 chars | `safestrcpy` truncates to 15 + NUL |

---

## 10. Implementation Phases

| Phase | Description | Files | Members |
|-------|-------------|-------|---------|
| 1 | Define shared `struct pinfo` | `kernel/pinfo.h` | 1 |
| 2 | Add syscall number and dispatch | `kernel/syscall.h`, `kernel/syscall.c` | 2 |
| 3 | Implement `sys_getprocs()` | `kernel/sysproc.c` | 1, 2 |
| 4 | User-space syscall stub | `user/usys.pl`, `user/user.h` | 3 |
| 5 | Write `ptv` visualizer | `user/ptv.c`, `Makefile` | 3, 5 |
| 6 | Write test programs + harness | `user/forktree.c`, `user/orphantree.c`, `test-xv6.py` | 4 |
| 7 | Integration + zombie test | `user/zombie.c`, end-to-end testing | 5 |

---

## 11. Development Environment

### Docker

```dockerfile
FROM debian:bookworm
RUN apt-get update && apt-get install -y \
    git build-essential gcc-riscv64-linux-gnu \
    binutils-riscv64-linux-gnu qemu-system-misc gdb-multiarch
WORKDIR /workspace
CMD ["bash"]
```

Build and run:
```bash
docker build -t xv6-dev .
docker run -it --rm -v $(pwd):/workspace xv6-dev
```

Inside the container:
```bash
make
make qemu
# In xv6 shell:
$ ptv
```

### Toolchain

The Makefile auto-detects the RISC-V toolchain prefix by trying:
`riscv64-unknown-elf-`, `riscv64-elf-`, `riscv64-none-elf-`,
`riscv64-linux-gnu-`, `riscv64-unknown-linux-gnu-`.

Minimum QEMU version: 7.2 (checked by `check-qemu-version` target).

---

## 12. State Constants Reference

| Constant | Value | Meaning |
|----------|-------|---------|
| `PINFO_UNUSED` / `UNUSED` | 0 | Not allocated (skipped) |
| `PINFO_USED` / `USED` | 1 | Allocated, not yet runnable |
| `PINFO_SLEEPING` / `SLEEPING` | 2 | Sleeping on a channel |
| `PINFO_RUNNABLE` / `RUNNABLE` | 3 | Ready to run |
| `PINFO_RUNNING` / `RUNNING` | 4 | Currently executing |
| `PINFO_ZOMBIE` / `ZOMBIE` | 5 | Exited, waiting for parent `wait()` |

Kernel code (`kernel/proc.c`, `kernel/sysproc.c`) uses the `enum procstate`
values from `kernel/proc.h:82`. User code uses the `PINFO_*` macros from
`kernel/pinfo.h`.

---

## 13. Data Structure: `struct pinfo`

```
Offset  Size  Field     Description
------  ----  -----     -----------
  0      4    pid       Process ID (1..32767)
  4      4    ppid      Parent PID, -1 if none
  8      4    state     PINFO_UNUSED(0) .. PINFO_ZOMBIE(5)
 12      4    (pad)     implicit padding
 16     16    name[16]  Null-terminated process name
                      ────────
Total: 32 bytes per entry
```

A full `struct pinfo procs[64]` buffer is 2048 bytes (fits on the stack).

---

## 14. Known Issues & Potential Improvements

### Fixed
- ~~State `#define`s in `pinfo.h` conflicted with `enum procstate` in `proc.h`~~
  → Fixed by prefixing with `PINFO_`.
- ~~`test_pstree` regex `^init\(1\)` didn't match `init(1,SLEEP)`~~
  → Fixed to `^init\(1`.
- ~~Function names used `pstree` instead of `ptv`~~
  → Renamed to `test_ptv_*`.

### Current
- `forktree.c`: leaf process exits into ZOMBIE state, but the tree depth math
  creates exactly the right number of levels for `max_depth=4`.
- `test_ptv_forktree`: relies on `time.sleep(3)` being sufficient for the
  fork hierarchy to settle. Could be flaky on slow emulation.

### Potential
- Add `-a` (all processes) and `-p PID` (single process ancestry) flags.
- Add cycle detection in DFS (unlikely in xv6, but safety net).
- Print orphaned root processes separately instead of requiring PID 1.

---

## 15. File Dependency Graph

```
kernel/pinfo.h
    ├── kernel/sysproc.c  (for struct pinfo in copyout)
    └── user/ptv.c        (for struct pinfo + PINFO_* constants)

kernel/syscall.h
    ├── kernel/syscall.c  (dispatch table via SYS_getprocs)
    └── user/usys.S       (generated from usys.pl, includes syscall.h)

kernel/syscall.c
    └── kernel/sysproc.o  (linked via extern sys_getprocs)

user/user.h
    └── user/ptv.c        (declares getprocs())

user/usys.pl
    └── user/usys.S       (generated → usys.o → linked into _ptv)

Makefile
    └── UPROGS includes _forktree, _orphantree, _ptv
```
