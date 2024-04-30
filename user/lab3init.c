// init: The initial user-level program
// We use `lab3init.c` as the initial user program for a bit until
// students have implemented `sbrk` (which is necessary because the
// eventual "final" `init.c` execs `sh.c`, which calls `malloc` (and
// thus requires `sbrk`)).

#include <cdefs.h>
#include <fcntl.h>
#include <stat.h>
#include <user.h>

char *argv[] = {"lab3test", 0};

int main(void) {
  if (open("console", O_RDWR) < 0) {
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0); // stdout
  dup(0); // stderr
  printf(1, "lab3init running\n");
  exec("lab3test", argv);
  printf(1, "lab3init: exec failed\n");
  exit();
}
