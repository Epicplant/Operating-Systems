#include <cdefs.h>
#include <fcntl.h>
#include <fs.h>
#include <param.h>
#include <stat.h>
#include <syscall.h>
#include <trap.h>
#include <user.h>
#include <test.h>

void run_test(char*);
void overwrite_test(void);
void append_test(void);
void create_small(void);
void create_large(void);
void one_file_test(void);
void four_files_test(void);
void unlink_basic(void);
void unlink_bad_case(void);
void unlink_open(void);
void unlink_inum_reuse(void);
void unlink_data_cleanup(void);

char buf[8192];

int main(int argc, char *argv[]) {
  char cmd[40];

  while (true) {
    shell_prompt("lab4.a");
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
    overwrite_test();
    append_test();
    create_small();
    create_large();
    one_file_test();
    four_files_test();
    // requires create to be implemented
    unlink_basic();
    unlink_bad_case();
    unlink_open();
    unlink_inum_reuse();
    unlink_data_cleanup();
    pass("lab4 part a tests");
  } else if (strcmp(test, "exit\n") == 0) {
    exit();
  } else if (strcmp(test, "overwrite_test\n") == 0) {
    overwrite_test();
  } else if (strcmp(test, "append_test\n") == 0) {
    append_test();
  } else if (strcmp(test, "create_small\n") == 0) {
    create_small();
  } else if (strcmp(test, "create_large\n") == 0) {
    create_large();
  } else if (strcmp(test, "one_file_test\n") == 0) {
    one_file_test();
  } else if (strcmp(test, "four_files_test\n") == 0) {
    four_files_test();
  } else if (strcmp(test, "unlink_basic\n") == 0) {
    unlink_basic();
   } else if (strcmp(test, "unlink_bad_case\n") == 0) {
    unlink_bad_case();
  } else if (strcmp(test, "unlink_open\n") == 0) {
    unlink_open();
  } else if (strcmp(test, "unlink_inum_reuse\n") == 0) {
    unlink_inum_reuse();
  } else if (strcmp(test, "unlink_data_cleanup\n") == 0) {
    unlink_data_cleanup();
  } else {
    printf(stderr, "input matches no test: %s" , test);
  }
}

void overwrite_test(void) {
  test("overwrite_test");

  int fd;

  strcpy(buf, "otter was here\n");
  fd = open("small.txt", O_RDWR);
  if (fd < 0) {
    error("overwrite_test: could not open small.txt with read write permissions");
  }

  // overwrite the original data.
  int n = write(fd, buf, strlen(buf) + 1);
  if (n != strlen(buf) + 1) {
    error("overwrite_test: did not write entire buffer, requsted %d bytes, wrote %d bytes", strlen(buf) + 1, n);
  }
  assert(close(fd) == 0);

  fd = open("small.txt", O_RDONLY);
  if (fd < 0) {
    error("overwrite_test: could not open small.txt with read only permissions");
  }

  assert(read(fd, buf, n) == n);
  if (strcmp(buf, "otter was here\n") != 0) {
    error("overwrite_test: file content incorrect, expecting 'otter was here', got '%s'", buf);
  }
  assert(close(fd) == 0);

  pass("");
}

void append_test(void) {
  test("append_test");

  int fd, len;
  uint old_size;
  struct stat st;

  fd = open("small.txt", O_RDWR);
  if (fd < 0) {
    error("append_test: could not open small.txt with RW permissions");
  }

  if (fstat(fd, &st) != 0) {
    error("append_test: could not stat small.txt");
  }
  old_size = st.size;

  int n = read(fd, buf, old_size);
  if (n != old_size) {
    error("append_test: failed to read all %d bytes, read %d", old_size, n);
  }

  // overwrite the last char, and append data.
  strcpy(buf, "another otter was here\n");
  len = strlen(buf) + 1;
  if ((n = write(fd, buf, len)) != len) {
    error("append_test: did not write entire buffer, requested %d bytes, wrote: %d\n", len, n);
  }
  if (fstat(fd, &st) != 0) {
    error("append_test: could not stat small.txt after a write");
  }
  if (st.size != old_size + len) {
    error("append_test: appended file does not have the correct length, expecting %d, got %d bytes", old_size+len, st.size);
  }
  assert(close(fd) == 0);

  // reopen and make sure the appended content is there
  fd = open("small.txt", O_RDONLY);
  if (fd < 0) {
    error("append_test: could not open small.txt with read only permissions");
  }
  assert(read(fd, buf, old_size) == old_size); // read to previous end
  assert(read(fd, buf, len) == len);
  if (strcmp(buf, "another otter was here\n") != 0) {
    error("append_test: file content did not match expected, got: '%s'", buf);
  }
  assert(close(fd) == 0);

  pass("");
}

// Creates a new file.
// Writes 1 byte to it, reads 1 byte from it.
void create_small(void) {
  test("create_small");

  int fd, n;

  if ((fd = open("create.txt", O_CREATE | O_RDWR)) < 0) {
    error("create_small: create 'create.txt' failed");
  }
  assert(close(fd) == 0);

  // Reopen and write 1 byte.
  if ((fd = open("create.txt", O_RDWR)) < 0) {
    error("create_small: failed to open a freshly created file 'create.txt'");
  }

  memset(buf, 1, 1);
  n = write(fd, buf, 1);
  if (n != 1) {
    error("create_small: error writing to the created file");
  }
  assert(close(fd) == 0);

  // reopen and read 1 byte, file already exists, should not allocate a new file
  if ((fd = open("create.txt", O_CREATE | O_RDONLY)) < 0) {
    error("create_small: failed to open a freshly created file 'create.txt'");
  }

  memset(buf, 0, 1);
  n = read(fd, buf, 1);
  if (n != 1) {
    error("create_small: error reading from created file");
  }
  // ensure read got the correct value.
  assert(buf[0] == 1);
  assert(close(fd) == 0);

  pass("");
}

void create_large(void) {
  test("create_large");

  int fd, i, n;

  if ((fd = open("create_large.txt", O_CREATE | O_RDWR)) < 0) {
    error("create_large: create 'create_large.txt' failed");
  }

  memset(buf, 2, sizeof(buf));
  for (i=0; i<10; i++) {
    // requested write size spans 16 sectors
    n = write(fd, buf, sizeof(buf));
    if (n != sizeof(buf)) {
      error("create_large: failed to write %d bytes to create_large.txt, wrote %d", sizeof(buf), n);
    }
  }
  assert(close(fd) == 0);

  // reopen and make sure the content was written
  if ((fd = open("create_large.txt", O_RDONLY)) < 0) {
    error("create_large: failed to open a freshly created file 'create_large.txt'");
  }

  memset(buf, 0, sizeof(buf));
  for (i=0; i<10; i++) {
    // requested write size spans 16 sectors
    n = read(fd, buf, sizeof(buf));
    if (n != sizeof(buf)) {
      error("create_large: failed to read %d bytes from create_large.txt, read %d", sizeof(buf), n);
    }

    for (n=0; n<sizeof(buf); n++) {
      if (buf[n] != 2) {
        error("create_large: byte %d has the wrong value, expecting 2, got %d", i*sizeof(buf)+n, buf[n]);
      }
    }
  }
  assert(close(fd) == 0);

  pass("");
}

// Creates a file, writes and reads data.
// Data is written and read by 500 bytes to try
// and catch errors in writing.
void one_file_test(void) {
  test("one_file_test");

  int fd, n, i, j;

  if ((fd = open("onefile.txt", O_CREATE|O_RDWR)) < 0) {
    error("one_file_test: failed to create 'onefile.txt'");
  }
  assert(close(fd) == 0);

  if ((fd = open("onefile.txt", O_RDWR)) < 0) {
    error("one_file_test: failed to open 'onefile.txt' after its creation");
  }
  memset(buf, 0, sizeof(buf));
  for (i = 0; i < 10; i++) {
    memset(buf, i, 500);
    if ((n = write(fd, buf, 500)) != 500) {
      error("one_file_test: requested to write 500 bytes, wrote %d bytes at iteration %d", n, i);
    }
  }
  assert(close(fd) == 0);

  if ((fd = open("onefile.txt", O_RDONLY)) < 0) {
    error("one_file_test: failed to open 'onefile.txt' after its creation");
  }
  memset(buf, 0, sizeof(buf));
  for (i = 0; i < 10; i++) {
    if ((n = read(fd, buf, 500)) != 500) {
      error("one_file_test: requested to read 500 bytes, read %d bytes at iteration %d", n, i);
    }
    for (j = 0; j < 500; j++) {
      if (i != buf[j]) {
        error("one_file_test: expected to read %d for buf[%d], instead read %d", i, j, buf[j]);
      }
    }
  }
  assert(close(fd) == 0);

  pass("");
}

// four processes write to different files to test concurrent block allocation.
void four_files_test(void) {
  test("four_files_test");

  int fd, pid, i, j, n, total, pi;
  int num = 4;
  char *names[] = {"f0", "f1", "f2", "f3"};
  char *fname;

  // create 4 children, each child creates a file and write to their own file (f0-f3)
  // child 0 fills the file content with 0s, child 1 with 1s and so on
  for (pi = 0; pi < num; pi++) {
    fname = names[pi];
    pid = fork();
    if (pid < 0) {
      error("four_files_test: failed to create child %d", pi);
    }

    if (pid == 0) {
      fd = open(fname, O_CREATE | O_RDWR);
      if (fd < 0) {
        error("four_files_test: child %d failed to create file %s", pi, fname);
      }

      memset(buf, '0' + pi, 500); // each child writes different value
      for (i = 0; i < 12; i++) {
        if ((n = write(fd, buf, 500)) != 500) {
          error("four_files_test: child %d failed to write 500 bytes to file %s, only wrote %d", pi, fname, n);
        }
      }
      exit();
      error("four_files_test: child %d failed to exit", pi);
    }
  }

  // wait for all children to exit
  for (pi = 0; pi < num; pi++) {
    assert(wait() > 0);
  }

  // check that each child's file has the correct content
  for (i = 0; i < num; i++) {
    fname = names[i];
    fd = open(fname, O_RDONLY);
    if (fd < 0) {
      error("four_files_test: failed to open file%s", fname);
    }

    total = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
      for (j = 0; j < n; j++) {
        if (buf[j] != '0'+i) {
          error("four_files_test: file %s has the wrong value at byte %d, was %d, should be %d", fname, j, buf[j], '0'+i);
        }
      }
      total += n;
    }
    assert(close(fd) == 0);

    if (total != 12 * 500) {
      error("four_files_test: expecting to read %d bytes from file %s, only got %d", 12*500, fname, total);
    }
  }

  pass("");
}

// check for unlink failure cases
void unlink_bad_case(void) {
  test("unlink_bad_case");

  if (unlink("nonexistent_file") != -1) {
    error("unlink_bad_case: able to delete file that doesn't exist");
  }

  if (unlink("..") != -1 || unlink(".") != -1) {
    error("unlink_bad_case: able to unlink non empty directories");
  }

  // pass in bad param
  if (unlink((char*) 0x7777beef) != -1) {
    error("unlink_bad_case: able to unlink with a random user address");
  }

  // pass in bad param
  if (unlink((char*) KERNBASE+0x100000) != -1) {
    error("unlink_bad_case: able to unlink with a random kernel address");
  }

  pass("");
}

void unlink_basic(void) {
  test("unlink_basic");

  int fd;
  char* name = "to_be_deleted";
  
  fd = open(name, O_RDWR | O_CREATE);
  if (fd < 0) {
    error("unlink_basic: unable to create file %s", name);
  }
  assert(close(fd) == 0);

  if (unlink(name) < 0) {
    error("unlink_basic: unable to delete newly created file %s", name);
  }

  fd = open(name, O_RDWR);
  if (fd != -1) {
    error("unlink_basic: able to reopen deleted file %s", name);
  }

  pass("");
}

void unlink_open(void) {
  test("unlink_open");

  int fd, fd2, pid;
  char* name;
  struct stat stat;

  name = "some_other_file";
  fd = open(name, O_RDWR | O_CREATE);
  if (fd < 0) {
    error("unlink_open: unable to create file %s", name);
  }

  if (unlink(name) != -1) {
    error("unlink_open: able to delete file that's still open");
  }

  if (close(fd) == -1) {
    error("unlink_open: unable to close file");
  }

  fd = open(name, O_RDWR);
  if (fd < 0) {
    error("unlink_open: unable to open newly created file %s", name);
  }

  pid = fork();
  if (pid < 0) {
    error("unlink_open: fork failed");
  }

  if (pid == 0) {
    // child closes the file and tries to delete it
    assert(close(fd) == 0); 
    if (unlink(name) != -1) {
      error("unlink_open: able to delete file that's open in another process");
    }
    exit();
  }

  // parent opens another file, unlink shouldn't interfere with other opens
  fd2 = open("small.txt", O_RDWR);
  if (fd2 < 0) {
    error("unlink_open: unable to open another file");
  }
  assert(fstat(fd2, &stat) == 0);

  assert(wait() == pid);
  assert(close(fd) == 0); // parent closes the file, all references should be gone

  if (unlink(name) == -1) {
    error("unlink_open: unable to delete file %s once all references closed", name);
  }

  if (open(name, O_RDWR) != -1) {
    error("unlink_open: able to open already-deleted file");
  }

  assert(stat.size <= sizeof(buf));
  if (read(fd2, buf, stat.size) != stat.size) {
    error("unlink_open: couldn't read from small after unlink");
  }
  assert(close(fd2) == 0);

  pass("");
}

// check inum usage/reuse and ordering
void unlink_inum_reuse(void) {
  test("unlink_inum_reuse");

  int i, fd;
  int fds[4];
  int inums[4];
  char *names[] = {"unlink0", "unlink1", "unlink2", "unlink3"};
  struct stat ss;

  // create 4 files, store their inum usage
  for (i=0; i<4; i++) {
    fds[i] = open(names[i], O_RDWR | O_CREATE);
    if (fds[i] < 0) {
      error("unlink_inum_reuse: unable to create %s", names[i]);
    }
    assert(fstat(fds[i], &ss) == 0);
    inums[i] = ss.ino;
    assert(close(fds[i]) == 0);
  }

  // check inum is allocated in order
  for (i=0; i<3; i++) {
    if (inums[i]+1 != inums[i+1]) {
      error("unlink_inum_reuse: inodes weren't allocated in order, file %s and %s should be one inum apart, \
        but got %d and %d instead", names[i], names[i+1], inums[i], inums[i+1]);
    }
  }

  // unlink a file and create a new file to test for inum reuse
  if (unlink(names[1]) < 0) {
    error("unlink_inum_reuse: failed to unlinked file %s", names[1]);
  }

  fd = open("new_file", O_RDWR | O_CREATE);
  if (fd < 0) {
    error("unlink_inum_reuse: unable to create new_file");
  }
  assert(fstat(fd, &ss) == 0);
  
  if (ss.ino != inums[1]) {
    error("unlink_inum_reuse: failed to reuse inode %d, newly created file has inum %d", inums[1], ss.ino);
  }

  assert(close(fd) == 0);
  assert(unlink("new_file") == 0);

  // delete other created files
  for (i=0; i<4; i++) {
    if (i != 1) {
      assert(unlink(names[i]) == 0);
    }
  }

  pass("");
}

// should not be able to read from a deleted file
void unlink_data_cleanup(void) {
  test("unlink_data_cleanup");

  int fd;
  
  fd = open("tmpfile", O_RDWR | O_CREATE);
  if (fd < 0) {
    error("unlink_data_cleanup: can't create tmpfile");
  }

  for (int i = 0; i < 8; ++i) {
    buf[i] = i + 1;
  }

  if (write(fd, buf, 8) != 8) {
    error("unlink_data_cleanup: can't write 8 bytes to tmpfile");
  }

  assert(close(fd) == 0);
  if (unlink("tmpfile") < 0) {
    error("unlink_data_cleanup: can't unlink tmpfile");
  }

  // create another file with the same name
  fd = open("tmpfile", O_RDWR | O_CREATE);
  if (fd < 0) {
    error("unlink_data_cleanup: can't create tmpfile");
  }
  // try to read from freshly created file
  memset(buf, 0, 8);
  if (read(fd, buf, 8) > 0) {
    error("unlink_data_cleanup: able to read from freshly created file");
  }

  assert(close(fd) == 0);
  if (unlink("tmpfile") < 0) {
    error("unlink_data_cleanup: can't unlink tmpfile");
  }

  pass("");
}
