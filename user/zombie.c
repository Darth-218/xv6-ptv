// Create a zombie process.
// Uses double-fork so the shell is not blocked:
//   zombie runs → grandparent exits → shell continues
//   parent stays alive → keeps grandchild as zombie for ptv to see.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int pid = fork();
  if (pid < 0)
    exit(1);

  if (pid == 0) {
    int pid2 = fork();
    if (pid2 < 0)
      exit(1);
    if (pid2 == 0)
      exit(0);          // Grandchild exits → zombie
    for (;;) pause(10); // Stay alive so grandchild stays zombie
  }

  exit(0);
}
