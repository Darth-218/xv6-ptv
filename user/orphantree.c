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
    if (pid2 == 0) {
      for (;;) pause(10);
    }
    exit(0);
  }

  wait(0);
  exit(0);
}
