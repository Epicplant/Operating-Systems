#include <cdefs.h>
#include <fcntl.h>
#include <stat.h>
#include <user.h>
#include <test.h>

void run_test(char*);
void concurrent_dup(void);
void concurrent_create(void);
void concurrent_write(void);
void concurrent_read(void);
void concurrent_delete(void);
void delete_stress_test(void);

int main(int argc, char *argv[]) {
  char cmd[40];

  while (true) {
    shell_prompt("lab4.b");
    memset(cmd, 0, sizeof(cmd));
    gets(cmd, sizeof(cmd));
    if (cmd[0] == 0) {
      continue;
    }
    run_test(cmd);
  }

  exit();
  return 0;
}

void run_test(char* test) {
  if (strcmp(test, "all\n") == 0) {
    concurrent_dup();
    concurrent_create();
    concurrent_write();
    concurrent_read();
    concurrent_delete();
    delete_stress_test();
    pass("lab4 part b tests");
  } else if (strcmp(test, "exit\n") == 0) {
    exit();
  } else if (strcmp(test, "concurrent_dup\n") == 0) {
    concurrent_dup();
  } else if (strcmp(test, "concurrent_create\n") == 0) {
    concurrent_create();
  } else if (strcmp(test, "concurrent_write\n") == 0) {
    concurrent_write();
  } else if (strcmp(test, "concurrent_read\n") == 0) {
    concurrent_read();
  } else if (strcmp(test, "concurrent_delete\n") == 0) {
    concurrent_delete();
  } else if (strcmp(test, "delete_stress_test\n") == 0) {
    delete_stress_test();
  } else {
    printf(stderr, "input matches no test: %s" , test);
  }
}

// create 50 children, each doing dup on a file opened by the parent
// requires proper sychronization of file refcount
void concurrent_dup(void) {
  test("concurrent_dup");

  int i, pid, fd1, fd2, fd3, fd4;

  fd1 = open("concurrent_dup.txt", O_CREATE | O_RDWR);
  if (fd1 < 0) {
    error("concurrent_dup: failed to create concurrent_dup.txt");
  }

  for (i = 0; i < 50; i++) {
    pid = fork();
    if (pid < 0) {
      error("concurrent_dup: fork() failed at iteration %d", i);
    }

    if (pid == 0) {
      fd2 = dup(fd1);
      assert(fd2 == fd1+1);
      fd3 = dup(fd1);
      assert(fd3 == fd1+2);
      // sleep(50);
      fd4 = dup(fd1);
      assert(fd4 == fd1+3);

      // close some fds for fun 
      // all fds should be closed when child exits
      assert(close(fd1) == 0);
      assert(close(fd2) == 0);
      exit();
    }
  }
  
  // wait for all children to finish
  for (i = 0; i < 50; i++) {
    assert(wait() > 0);
  }
  // no more children
  assert(wait() == -1);
  assert(close(fd1) == 0);

  // if refcount is correct, should be able to unlink right now
  if (unlink("concurrent_dup.txt") < 0) {
    error("concurrent_dup: failed to unlink concurrent_dup.txt after all references are closed");
  }

  pass("");
}

// fork 50 children, all trying to create the same file
// check that the inode they see are the same inode
void concurrent_create() {
  test("concurrent_create");

  int i, pid, fd;
  uint ino;
  int fds[2];
  struct stat stat;

  // start a pipe so that we can gather output from children
  if (pipe(fds) < 0) {
    error("concurrent_create: failed to create a pipe for parent child communication");
  }

  for (i = 0; i < 50; i++) {
    pid = fork();
    if (pid < 0) {
      error("concurrent_create: fork failed at iteration %d", i);
    }
    if (pid == 0) {
      assert(close(fds[0]) == 0); // child closes read end

      // all children trying to create cc_create.txt
      fd = open("cc_create.txt", O_CREATE | O_RDWR);
      if (fd < 0) {
        error("concurrent_create: child %d failed to create/open cc_create.txt", i);
      }
      assert(fstat(fd, &stat) == 0);
      assert(close(fd) == 0);

      // write the inumber to pipe
      assert(write(fds[1], &stat.ino, sizeof(stat.ino)) == sizeof(stat.ino));
      exit();
    }
  }

  // parent closes write end
  assert(close(fds[1]) == 0); 

  // wait for all children to exit
  for (i = 0; i < 50; i++) {
    assert(wait() > 0);
  }
  // no more children
  assert(wait() == -1);

  // time for parent to validate
  fd = open("cc_create.txt", O_RDONLY);
  assert(fd >= 0);
  assert(fstat(fd, &stat) == 0);
  assert(close(fd) == 0);

  for (i = 0; i < 50; i++) {
    // read inum sent from child processes
    assert(read(fds[0], &ino, sizeof(ino)) == sizeof(ino));
    // we should all have the same view of cc_create.txt
    assert(ino == stat.ino);
  }

  assert(read(fds[0], &ino, 1) == 0); // pipe should be empty now
  assert(close(fds[0]) == 0);

  // clean up the created file
  if (unlink("cc_create.txt") < 0) {
    error("concurrent_create: failed to unlink cc_create.txt after all references are closed");
  }

  pass("");
}

void concurrent_write() {
  test("concurrent_write");

  int i, j, pid, fd;
  int map[50];
  char buf[5];

  fd = open("cc_write.txt", O_CREATE | O_WRONLY);
  if (fd < 0) {
    error("concurrent_write: failed to create cc_write.txt");
  }

  for (i = 0; i < 50; i++) {
    pid = fork();
    if (pid < 0) {
      error("concurrent_write: fork failed at iteration %d", i);
    }
    if (pid == 0) {
      memset(buf, i+65, 5);
      // issue 20 write requests
      // each write should be atomic
      for (j = 0; j < 20; j++) {
        assert(write(fd, buf, 5) == 5);
      }
      exit();
    }
  }

  // wait for all children
  for (i = 0; i < 50; i++) {
    assert(wait() > 0);
  }
  // no more children
  assert(wait() == -1);
  assert(close(fd) == 0);

  // reopen and make sure writes come out atomically
  fd = open("cc_write.txt", O_RDONLY);
  assert(fd >= 0);

  memset(map, 0, sizeof(map));
  // read 5 bytes at a time
  for (i = 0; i < 1000; i++) {
    // read 5 bytes at a time
    assert(read(fd, buf, 5) == 5);
    // all 5 bytes should have the same value
    for (j = 0; j < 4; j++) {
      if (buf[j] != buf[j+1]) {
        error("concurrent_write: failed to atomically write 5 bytes at a time, got two values %d and %d", buf[j], buf[j+1]);
      }
    }
    map[buf[0] - 65] += 5;
  }

  // EOF
  assert(read(fd, buf, 1) == 0);
  assert(close(fd) == 0);

  for (i = 0; i < 50; i++) {
    if (map[i] != 100) {
      error("missing byte from child %d", i);
    }
  }

  pass("");
}

void concurrent_read() {
  test("concurrent_read");

  int i, j, pid, fd;
  uchar buf[20];

  fd = open("cc_read.txt", O_CREATE | O_WRONLY);
  assert(fd >= 0);

  // parent set up some content
  for (i = 0; i < 250; i++) {
    memset(buf, i, 20);
    assert(write(fd, buf, 20) == 20);
  }
  assert(close(fd) == 0);

  fd = open("cc_read.txt", O_RDONLY);
  assert(fd >= 0);

  buf[0] = 0;
  for (i = 0; i < 50; i++) {
    pid = fork();
    if (pid < 0) {
      error("concurrent_read: fork() failed at iteration %d", i);
    }
    if (pid == 0) {
      // each child reads 100 byte
      for (j = 0; j < 100; j++) {
        while (read(fd, buf + 1, 1) <= 0);
        if (buf[1] >= buf[0]) {
          buf[0] = buf[1];
        } else {
          error("concurrent_read: read out of order: read %d before %d in child %d", buf[0], buf[1], i);
        }
      }
      exit();
    } 
  }

  for (i = 0; i < 50; i++) {
    assert(wait() > 0);
  }
  // no more children
  assert(wait() == -1);
  // there shouldn't be anything else left to read
  assert(read(fd, buf, 1) == 0);
  assert(close(fd) == 0);

  pass("");
}

void concurrent_delete() {
  test("concurrent_delete");

  int fd, pid;
  char buf[2];
  int fds[2];

  assert(pipe(fds) == 0);

  fd = open("cc_delete.txt", O_RDWR | O_CREATE);
  assert(fd >= 0);
  assert(close(fd) == 0);

  for (int i = 0; i < 50; ++i) {
    pid = fork();
    if (pid < 0) {
      error("concurrent_delete: fork failed at iteration %d", i);
    }
    if (pid == 0) {
      assert(close(fds[0]) == 0); // child closes read end
      if (unlink("cc_delete.txt") == 0) {
        assert(write(fds[1], buf, 1) == 1); // write to pipe upon successful deletes
      }
      exit();
    }
  }

  assert(close(fds[1]) == 0); // parent closes the write end
  for (int i = 0; i < 50; ++i) {
    assert(wait() > 0);
  }
  
  assert(read(fds[0], buf, 1) == 1);
  if (read(fds[0], buf, 1) != 0) {
    error("concurrent_delete: file deleted by child more than once");
  }
  
  pass("");
}

void printBar(int count, bool backspace) {
  if (backspace) {
    for (int i = 0; i < 100 + 2; ++i) {
      printf(stdout, "\b");
    }
  }

  printf(stdout, "[");
  for (int i = 0; i < 100; ++i) {
    if (i < count) {
      printf(stdout, "#");
    } else {
      printf(stdout, " ");
    }
  }
  printf(stdout, "]");
}

void delete_stress_test() {
  test("delete_stress_test");

  int fd1, fd2, i, n;
  char* text;
  char buf[128];

  fd1 = open("ddf1", O_RDWR | O_CREATE);
  assert(fd1 >= 0);

  text = "This is the data that goes into the file created here.";
  n = write(fd1, text, strlen(text));
  if (n != strlen(text)) {
    error("delete_stress_test: write failed, expecting to write %d bytes, wrote %d", strlen(text), n);
  }
  assert(close(fd1) == 0);

  fd1 = open("ddf1", O_RDONLY);
  assert(fd1 >= 0);

  printf(stdout, "\nstarting delete stress test (this should take around 5-10 min)...\n");
  printBar(0, false);

  // This test ensures that you're reclaiming space on delete by
  // churning through the entire disk's worth of memory several times over
  // It's slow.
  //
  // If you want to speed this up for your own intermediate testing
  // 1) reduce the size of the disk (FSSIZE) in inc/params.h
  // 2) change the number of loop iterations to (FSSIZE / min_file_blocks) * 2
  //
  // Just make sure you test on the original parameters before submission!
  for (i = 0; i < 50000; ++i) {
    fd2 = open("file", O_RDWR | O_CREATE);
    if (fd2 < 0) {
      error("delete_stress_test: unable to open file at iteration %d", i);
    }
    if (close(fd2) < 0) {
      error("delete_stress_test: unable to close file at iteration %d", i);
    }
    if (unlink("file") < 0) {
      error("delete_stress_test: unable to unlink file at iteration %d", i);
    }
    if (i % 500 == 0) {
      printBar(i / 500, true);
    }
  }

  assert(read(fd1, &buf, n) == n);
  if (strcmp(buf, text) != 0) {
    error("delete_stress_test: wrong data after delete arounds of deletes: expecting %s, got %s", text, buf);
  }

  pass("");
}

