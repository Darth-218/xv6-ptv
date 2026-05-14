# Member 2 — Kernel System Call Developer

## Phase 2: Syscall Number and Dispatch Wiring

Member 1 has already implemented `sys_getprocs()` in `kernel/sysproc.c`.
Member 2 is responsible for registering it in the syscall dispatch system.

### Changes needed

**1. `kernel/syscall.h:22`** — Add syscall number

```c
#define SYS_getprocs 22
```

Append after `#define SYS_close 21` on line 22.

**2a. `kernel/syscall.c:103`** — Add extern declaration

```c
extern uint64 sys_getprocs(void);
```

Add after `extern uint64 sys_close(void);` (line 103).

**2b. `kernel/syscall.c:128`** — Add dispatch table entry

```c
[SYS_getprocs]  sys_getprocs,
```

Add after `[SYS_close] sys_close,` (line 128).

### What this does

- `syscall.h` assigns syscall number 22 to `getprocs`
- The `extern` in `syscall.c` makes the linker find `sys_getprocs()` in `sysproc.o`
- The `syscalls[]` array entry maps syscall number 22 → `sys_getprocs` function
- When a user program executes `ecall` with `a7 = 22`, the `syscall()` function in
  `kernel/syscall.c:131` dispatches to `sys_getprocs()`

### Verification

Build the kernel:
```
$ make kernel/kernel
```

No errors means the extern and dispatch entry are correctly wired.

### Current syscall interface (already implemented by Member 1)

```
Arguments:  a0 = user-space buffer address (uint64)
            a1 = max count (int)
Return:     a0 = number of processes copied, or -1 on error

Locking:    acquire(wait_lock) → per-process acquire(p->lock) → copyout() → release
```
