#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
fork_child(int depth, int max_depth)
{
  int pid;

  if (depth >= max_depth)
    exit(0);

  pid = fork();
  if (pid < 0)
    exit(1);

  if (pid == 0) {
    if (depth == max_depth - 1)
      exit(0);
    fork_child(depth + 1, max_depth);
    exit(0);
  }

  wait(0);
}

int
main(void)
{
  printf("forktree starting\n");
  fork_child(0, 4);
  printf("forktree done\n");
  exit(0);
}
