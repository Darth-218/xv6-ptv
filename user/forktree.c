// Create a persistent multi-level fork tree for ptv testing.
// Uses double-fork so the shell is not blocked:
//   forktree runs → grandparent exits → shell continues
//   parent stays alive with its fork hierarchy visible to ptv.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
fork_tree(int start_depth, int max_depth)
{
  int pid;

  for (int depth = start_depth; depth < max_depth - 1; depth++) {
    pid = fork();
    if (pid < 0)
      exit(1);
    if (pid == 0)
      continue;
    for (;;) pause(10);
  }

  pid = fork();
  if (pid < 0)
    exit(1);
  if (pid == 0)
    exit(0);
  for (;;) pause(10);
}

int
main(void)
{
  printf("forktree starting\n");
  int pid = fork();
  if (pid < 0)
    exit(1);
  if (pid == 0) {
    fork_tree(0, 4);
    for (;;) pause(10);
  }
  printf("forktree done\n");
  exit(0);
}
