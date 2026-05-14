#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/pinfo.h"

#define MAX_CHILDREN 64

struct child_list {
  int pids[MAX_CHILDREN];
  int count;
};

void
dfs(int pid, int depth, struct pinfo *procs, int n, struct child_list *children)
{
  for (int i = 0; i < depth; i++)
    printf("  ");
  if (depth > 0)
    printf("\342\224\224\342\224\200 ");

  for (int i = 0; i < n; i++) {
    if (procs[i].pid == pid) {
      if (procs[i].state == 5)  // ZOMBIE
        printf("%s(%d,ZOMBIE)\n", procs[i].name, pid);
      else
        printf("%s(%d)\n", procs[i].name, pid);
      break;
    }
  }

  for (int i = 0; i < children[pid].count; i++)
    dfs(children[pid].pids[i], depth + 1, procs, n, children);
}

int
main(void)
{
  struct pinfo procs[NPROC];
  struct child_list children[NPROC];
  int n = getprocs(procs, NPROC);

  if (n < 0) {
    printf("ptv: getprocs failed\n");
    exit(1);
  }

  for (int i = 0; i < NPROC; i++)
    children[i].count = 0;

  for (int i = 0; i < n; i++) {
    int ppid = procs[i].ppid;
    if (ppid >= 0 && ppid < NPROC) {
      struct child_list *cl = &children[ppid];
      if (cl->count < MAX_CHILDREN)
        cl->pids[cl->count++] = procs[i].pid;
    }
  }

  int visited = 0;
  for (int i = 0; i < n; i++) {
    if (procs[i].ppid == -1) {
      dfs(procs[i].pid, 0, procs, n, children);
      visited++;
    }
  }

  if (visited == 0)
    dfs(1, 0, procs, n, children);

  exit(0);
}
