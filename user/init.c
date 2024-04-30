// init: The initial user-level program
//
// This is the true final goal for our initial user program in `xk`.
// This program forks off a child to run a shell then calls `wait` in an
// infinite loop. Should the child running the shell exit for any reason,
// this proc will re-fork and exec the shell again (ad infinitum).

#include <cdefs.h>
#include <fcntl.h>
#include <stat.h>
#include <user.h>

char *argv[] = {"sh", 0};

int main(void) {
  int pid, wpid;

  if (open("console", O_RDWR) < 0) {
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0); // stdout
  dup(0); // stderr

  for (;;) {
    printf(1, "init: starting a new shell\n");
    pid = fork();
    if (pid < 0) {
      printf(1, "init: fork failed\n");
      exit();
    }
    if (pid == 0) {
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    while ((wpid = wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");
  }
}
