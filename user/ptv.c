#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/pinfo.h"

void
dfs(int pid, int depth, struct pinfo *procs, int n)
{
  for (int i = 0; i < depth; i++)
    printf("  ");
  if (depth > 0)
    printf("\342\224\224\342\224\200 ");

  for (int i = 0; i < n; i++) {
    if (procs[i].pid == pid) {
      if (procs[i].state == 5)
        printf("%s(%d,ZOMBIE)\n", procs[i].name, pid);
      else
        printf("%s(%d)\n", procs[i].name, pid);
      break;
    }
  }

  for (int i = 0; i < n; i++)
    if (procs[i].ppid == pid)
      dfs(procs[i].pid, depth + 1, procs, n);
}

int
main(void)
{
  struct pinfo procs[NPROC];
  int n = getprocs(procs, NPROC);

  if (n < 0) {
    printf("ptv: getprocs failed\n");
    exit(1);
  }

  int visited = 0;
  for (int i = 0; i < n; i++) {
    if (procs[i].ppid == -1) {
      dfs(procs[i].pid, 0, procs, n);
      visited++;
    }
  }

  if (visited == 0)
    dfs(1, 0, procs, n);

  exit(0);
}
