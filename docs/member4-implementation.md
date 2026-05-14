# Member 4 — Testing & Validation Engineer

## Phase 6: Test Programs and Automation

### Test Program: `user/forktree.c` (fork chain + zombie)

A multi-level process hierarchy that exercises the full tree display:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void fork_child(int depth, int max_depth) {
    if (depth >= max_depth)
        exit(0);
    
    int pid = fork();
    if (pid < 0) {
        exit(1);
    }
    if (pid == 0) {
        // child: fork more or become zombie
        if (depth == max_depth - 1) {
            // leaf: exit immediately → zombie
            exit(0);
        }
        fork_child(depth + 1, max_depth);
        exit(0);
    }
    // parent: wait for child
    wait(0);
}

int main(void) {
    // Create a 4-level tree: init→forktree→child→grandchild(zombie)
    printf("forktree starting\n");
    fork_child(0, 4);
    printf("forktree done\n");
    exit(0);
}
```

### Test Program: `user/orphantree.c`

Test that orphaned processes are reparented to init correctly in the tree:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    int pid = fork();
    if (pid < 0) {
        exit(1);
    }
    if (pid == 0) {
        // child: fork a grandchild
        int pid2 = fork();
        if (pid2 < 0)
            exit(1);
        if (pid2 == 0) {
            // grandchild: pause to stay alive
            for (;;) pause(10);
        }
        // child exits — grandchild becomes orphan, reparented to init
        exit(0);
    }
    // parent: wait for child, then exit
    wait(0);
    exit(0);
}
```

### Test Automation: `test-xv6.py` additions

Add the following functions to `test-xv6.py`:

```python
def test_pstree():
    """Boot xv6, run pstree, validate process tree output."""
    q = QEMU(True)
    q.cmd("pstree\n")
    q.monitor(r'^init\(1\)', timeout=10)
    q.read()
    
    lines = q.lines()
    # verify init is the root
    if not any(re.match(r'^init\(1\)', l) for l in lines):
        print("FAIL: init(1) not found in pstree output")
        q.stop()
        sys.exit(1)
    print("OK: init(1) found")
    
    # verify sh is a child of init
    if not any(re.match(r'.*sh\(', l) for l in lines):
        print("FAIL: sh not found in pstree output")
        q.stop()
        sys.exit(1)
    print("OK: sh found")
    
    q.stop()

def test_pstree_forktree():
    """Run forktree test then pstree, verify multi-level hierarchy."""
    q = QEMU(True)
    q.cmd("forktree\n")
    time.sleep(3)
    q.cmd("pstree\n")
    time.sleep(1)
    q.read()
    
    lines = q.lines()
    # should show forktree and its children in tree
    found = any(re.search(r'forktree', l) for l in lines)
    if found:
        print("OK: forktree found in pstree output")
    else:
        print("FAIL: forktree not found")
    
    q.stop()

def test_pstree_orphans():
    """Run orphantree test, verify orphans shown under init."""
    q = QEMU(True)
    q.cmd("orphantree\n")
    time.sleep(3)
    q.cmd("pstree\n")
    time.sleep(1)
    q.read()
    
    lines = q.lines()
    # orphantree's grandchild should appear under init
    found = any(re.search(r'orphantree', l) for l in lines)
    if found:
        print("OK: orphaned process shown in tree")
    else:
        print("FAIL: orphan not found")
    
    q.stop()

def test_pstree_zombies():
    """Verify zombie processes are marked in the tree."""
    q = QEMU(True)
    # existing zombie test program
    q.cmd("zombie\n")
    time.sleep(2)
    q.cmd("pstree\n")
    time.sleep(1)
    q.read()
    
    lines = q.lines()
    zombie_marked = any(re.search(r'ZOMBIE', l) for l in lines)
    if zombie_marked:
        print("OK: zombie process marked in tree")
    else:
        print("FAIL: no zombie marker found")
    
    q.stop()
```

Register in `main()` so tests run via:
```
./test-xv6.py pstree
./test-xv6.py pstree_forktree
./test-xv6.py pstree_orphans
./test-xv6.py pstree_zombies
```

### Test scenarios summary

| Test | What it validates |
|------|-------------------|
| `pstree` | Basic tree: init(1) root + sh(2) child |
| `pstree_forktree` | Multi-level hierarchy renders correctly |
| `pstree_orphans` | Orphaned processes reparented to init |
| `pstree_zombies` | Zombie processes marked with ZOMBIE label |
