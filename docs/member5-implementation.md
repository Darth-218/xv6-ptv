# Member 5 — Advanced Visualization & Integration

## Role: Visualizer Enhancements + End-to-End Integration

Member 5 works alongside Member 3 on `user/ptv.c` and helps Member 4
validate the full pipeline.

### Visualizer enhancements

**1. State-aware formatting** — `user/ptv.c`

Enhance the DFS output to show process state:

```c
static const char *state_str[] = {
    [1] "USED",
    [2] "SLEEPING",
    [3] "RUNNABLE",
    [4] "RUNNING",
    [5] "ZOMBIE",
};

void print_process(struct pinfo *p) {
    if (p->state == ZOMBIE)
        printf("%s(%d,ZOMBIE)", p->name, p->pid);
    else if (p->state == RUNNING)
        printf("%s(%d,RUN)", p->name, p->pid);
    else if (p->state == SLEEPING)
        printf("%s(%d,SLEEP)", p->name, p->pid);
    else
        printf("%s(%d)", p->name, p->pid);
}
```

**2. Process count summary**

Print a footer line after the tree:
```
Total: 5 processes
```

**3. Large tree handling**

- The `children[]` array indexes by PID. Since PID can exceed NPROC-1,
  guard against out-of-bounds access.
- Use a linear scan approach instead of PID-indexed lookup for safety:
  ```c
  // find children of parent_id
  for (int i = 0; i < n; i++) {
      if (procs[i].ppid == parent_id) {
          dfs(procs[i].pid, depth + 1, procs, n);
      }
  }
  ```

  This approach is O(n^2) but n ≤ 64 (NPROC), so performance is fine.

### Integration checklist

| Step | Action | Verification |
|------|--------|--------------|
| 1 | Build kernel | `make kernel/kernel` succeeds |
| 2 | Build filesystem | `make fs.img` succeeds |
| 3 | Boot QEMU | `make qemu` boots to shell |
| 4 | Run ptv | `ptv` prints tree |
| 5 | Run zombie test | `zombie` then `ptv` shows ZOMBIE |
| 6 | Run forktree test | `forktree` then `ptv` shows hierarchy |
| 7 | Run orphantree test | `orphantree` then `ptv` shows orphans |
| 8 | Stress test | Run `ptv` repeatedly while other programs run |

### Common debugging scenarios

**`getprocs` returns -1:**
- Check that `SYS_getprocs` is defined in `syscall.h` and matches dispatch table
- Verify `sys_getprocs()` is linked: check for `sys_getprocs` in `kernel/kernel.sym`
- Ensure `wait_lock` and `proc[]` are properly extern'd in `sysproc.c`

**Garbage in `struct pinfo` fields:**
- Check `copyout()` destination offset: `buf + count * sizeof(struct pinfo)`
- Verify `safestrcpy` null-terminates `pi.name` (it does, with max 16)
- Ensure user-side buffer is large enough: `struct pinfo procs[NPROC]`

**Tree output is wrong:**
- Verify `ppid` values: orphaned processes get `ppid = 1` via `reparent()`
- Verify init's `ppid` comes back as `-1` (not `0`)
- Check that `UNUSED` processes are filtered out by the kernel

### Concurrent safety validation

The kernel locking protocol is:
```
acquire(wait_lock)
  for each proc:
    acquire(p->lock)
    read pid, state, name, parent
    release(p->lock)
    copyout()
release(wait_lock)
```

This ensures:
- `wait_lock` prevents `parent` pointer from being modified (by `reparent()`/`kexit()`)
- `p->lock` ensures consistent reads of `state`, `pid`, `name`
- Between `release(p->lock)` and the next `acquire`, other CPUs can modify processes,
  but the data already captured in the local `struct pinfo` is safe for `copyout()`
