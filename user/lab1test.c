#include <cdefs.h>
#include <fcntl.h>
#include <param.h>
#include <stat.h>
#include <stdarg.h>
#include <user.h>
#include <test.h>

void open_basic(void);
void open_bad_args(void);
void open_mismatch_perm(void);

void close_basic(void);
void close_bad_args(void);
void open_close_fd_limit(void);

void read_basic(void);
void read_bad_args(void);
void read_mismatch_perm(void);

void write_basic(void);
void write_bad_args(void);
void write_mismatch_perm(void);

void fstat_test(void);
void dup_test(void);

int main() {
  
  if(open("console", O_RDWR) < 0){
    error("lab1test: failed to open the console");
  }
  if (dup(0) < 0) {  // stdout
    error("lab1test: failed to dup stdin to stdout");
  }
  if (dup(0) < 0) {  // stderr
    error("lab1test: failed to dup stdin to stderr");
  }

  open_basic();
  open_bad_args();
  open_mismatch_perm(); // NOTE(lab4): comment this out once lab4 is implemented

  close_basic();
  close_bad_args();
  open_close_fd_limit();

  read_basic();
  read_bad_args();
  read_mismatch_perm();

  write_basic();
  write_bad_args();
  write_mismatch_perm();

  fstat_test();
  dup_test();

  pass("lab1 tests!");
  exit();
  return 0;
}

// basic open test
void open_basic(void) {
  test("open_basic");

  int fd, fd2;
  fd = open("/small.txt", O_RDONLY);
  if (fd < 0) {
    error("open_basic: cannot open small.txt, got fd %d", fd);
  }

  fd2 = open("/small.txt", O_RDONLY);
  if (fd2 < 0) {
    error("open_basic: cannot open small.txt on 2nd try, got fd %d", fd);
  }
  if (fd == fd2) {
    error("open_basic: got the same file descriptor from different open calls: %d, %d", fd, fd2);
  }

  pass("");
}

// invalid open arguments test
void open_bad_args(void) {
  test("open_bad_args");

  if (open("/doesnotexist.txt", O_RDONLY) != -1) {
    error("open_bad_args: able to open a file that doesn't exist");
  }

  if (open((char*)0x0, O_RDONLY) != -1) {
    error("open_bad_args: able to open with an invalid path address (0x0) from the user space");
  }

  if (open((char*)0xFFFF0000, O_RDONLY) != -1) {
    error("open_bad_args: able to open with an invalid path address (0xFFFF0000) from the user space");
  }

  if (open((char*)0xFFFFFFFF80000000, O_RDONLY) != -1) {
    error("open_bad_args: able to open with an invalid path address (0xFFFFFFFF80000000) from the kernel space");
  }

  pass("");
}

// open file with wrong permissions
void open_mismatch_perm(void) {
  test("open_mismatch_perm");

  if (open("/other.txt", O_CREATE) != -1) {
    error("open_mismatch_perm: able to create a file in a read only file system");
  }

  if (open("/small.txt", O_RDWR) != -1 || open("/small.txt", O_WRONLY) != -1) {
    error("open_mismatch_perm: able to open a file for writing in a read only file system");
  }

  pass("");
}

// basic close test
void close_basic(void) {
  test("close_basic");

  int fd;
  fd = open("/small.txt", O_RDONLY);
  if (fd < 0) {
    error("close_basic: cannot open small.txt, got fd %d", fd);
  }

  // close a valid fd, should succeed
  if (close(fd) < 0) {
    error("close_basic: failed to close file with fd %d", fd);
  }

  // close the same fd twice, should fail
  if (close(fd) != -1) {
    error("close_basic: able to close the same fd %d twice", fd);
  }

  pass("");
}

// invalid args for close
void close_bad_args(void) {
  test("close_bad_args");

  if (close(-1) != -1) {
    error("close_bad_args: able to close negative fd");
  }

  if (close(48) != -1) {
    error("close_bad_args: able to close non open file");
  }

  if (close(640) != -1) {
    error("close_bad_args: able to close fd that exceeds the number of open files");
  }

  pass("");
}

void open_close_fd_limit(void) {
  test("open_close_fd_limit");

  int fd, tmpfd, badfd;
  assert((fd = open("/small.txt", O_RDONLY)) >= 0);

  for (tmpfd = fd + 1; tmpfd < NOFILE; tmpfd++) {
    int newfd = open("/small.txt", O_RDONLY);
    if (newfd != tmpfd) {
      error("open_close_fd_limit: returned fd %d from open, was not the smallest free fd %d", newfd, tmpfd);
    }
  }

  // open should fail as we have reached the NOFILE limit
  if ((badfd = open("/small.txt", O_RDONLY)) != -1) {
    error("open_close_fd_limit: opened more files than allowed, returned fd %d", badfd);
  }

  // open should work once we close an open file
  assert(close(NOFILE-1) == 0);
  tmpfd = open("/small.txt", O_RDONLY);
  if (tmpfd < 0) {
    error("open_close_fd_limit: unable to open file after an fd is available, return value was %d", tmpfd);
  }

  assert(tmpfd == NOFILE-1);
  for (tmpfd = fd; tmpfd < NOFILE; tmpfd++) {
    if (close(tmpfd) != 0) {
      error("open_close_fd_limit: unable to close opened file, failed to close fd %d", tmpfd);
    }
  }

  pass("");
}

void read_basic(void) {
  test("read_basic");

  int fd, i;
  char buf[11];

  // test read only funcionality
  fd = open("/small.txt", O_RDONLY);
  if (fd < 0) {
    error("read_basic: unable to open small.txt");
  }

  if ((i = read(fd, buf, 10)) != 10) {
    error("read_basic: read of first 10 bytes unsucessful, read %d bytes", i);
  }

  buf[10] = 0;
  if (strcmp(buf, "aaaaaaaaaa") != 0) {
    error("read_basic: buf was not 10 a's, was: '%s'", buf);
  }

  if ((i = read(fd, buf, 10)) != 10) {
    error("read_basic: read of second 10 bytes unsucessful, read %d bytes", i);
  }

  buf[10] = 0;
  if (strcmp(buf, "bbbbbbbbbb") != 0) {
    error("read_basic: buf was not 10 b's, was: '%s'", buf);
  }

  // only 25 byte file
  if ((i = read(fd, buf, 10)) != 6) {
    error("read_basic: read of last 6 bytes unsucessful, read %d bytes", i);
  }

  buf[6] = 0;
  if (strcmp(buf, "ccccc\n") != 0) {
    error("read_basic: buf was not 5 c's (and a newline), was: '%s'", buf);
  }

  if (read(fd, buf, 10) != 0) {
    error("read_basic: able to read beyond the end of the file, read %d bytes", i);
  }

  if (close(fd) != 0) {
    error("read_basic: error closing fd %d", fd);
  }

  pass("");
}

void read_bad_args(void) {
  test("read_bad_args");

  int fd, i;
  char buf[11];

  if (read(800, buf, 11) != -1) {
    error("read_bad_args: able to read on an invalid file descriptor 800");
  }

  if (read(NOFILE-1, buf, 11) != -1) {
    error("read_bad_args: able to read on a non existent file descriptor %d", NOFILE-1);
  }

  // open a valid file to test invalid read params
  if ((fd = open("/small.txt", O_RDONLY)) < 0) {
    error("read_bad_args: unable to open small.txt");
  }

  if ((i = read(fd, buf, -100)) != -1) {
    error("read_bad_args: able to read negative number of bytes, read %d bytes", i);
  }

  if (read(fd, (char *)0xffffff00, 10) != -1) {
    error("read_bad_args: able to read to an invalid buffer (0xffffff00)");
  }

  assert(close(fd) == 0);
  pass("");
}

void read_mismatch_perm(void) {
  test("read_mismatch_perm");

  int fd;
  char buf[11];

  fd = open("console", O_WRONLY);
  assert(fd >= 0);

  if (read(fd, buf, 10) != -1) {
    // NOTE: If the test hangs here it's because you're trying to read from stdin (hint: shouldn't happen).
    error("read_mismatch_perm: able to read from console that's opened as write only");
  }

  assert(close(fd) == 0);
  pass("");
}

void write_basic(void) {
  test("write_basic");

  int len, i;
  char buf[11];

  strcpy(buf, "world ");
  len = strlen(buf);

  if ((i = write(stdout, buf, len)) != len) {
    error("write_basic: wasn't able to write all %d bytes to stdout, wrote %d bytes ", len, i);
  }

  strcpy(buf, "hello ");
  len = strlen(buf);

  if ((i = write(stdout, buf, len)) != len) {
    error("write_basic: unable to write %d bytes to stdout, wrote %d bytes", len, i);
  }

  pass("");
}

void write_bad_args(void) {
  test("write_bad_args");

  char buf[11];

  if (write(1000, buf, 11) != -1) {
    error("write_bad_args: able to write to an invalid file descriptor (1000)");
  }

  if (write(stdout, (char *)0xfffffbeef, 11) != -1) {
    error("write_bad_args: able to write with an invalid read buffer (0xfffffbeef)");
  }

  pass("");
}

void write_mismatch_perm(void) {
  test("write_mismatch_perm");

  int fd;
  char buf[11];

  assert((fd = open("console", O_RDONLY)) >= 0);
  if (write(fd, buf, 11) != -1) {
    error("write_mismatch_perm: able to write to console that's opened for read only");
  }
  assert(close(fd) == 0);
  pass("");
}

void fstat_test(void) {
  test("fstat_test");

  struct stat st;

  // invalid arg checks
  if (fstat(1111, &st) != -1) {
    error("fstat_test: able to fstat on a non existent file descriptor");
  }

  if (fstat(stdout, (struct stat *)0xffffffeed) != -1) {
    error("fstat_test: able to fstat with invalid struct stat address");
  }

  // fstat the console
  if (fstat(stdout, &st) != 0) {
    error("fstat_test: couldn't fstat on stdout");
  }

  assert(st.type == T_DEV);
  assert(st.size == 0);

  // stat a file, stat (ulib.c) is a wrapper for open, fstat, close
  if (stat("/small.txt", &st) != 0) {
    error("fstat_test: couldn't stat on '/small.txt'");
  }

  assert(st.type == T_FILE);
  assert(st.size == 26);

  pass("");
}

void dup_test(void) {
  test("dup_test");

  int fd1, fd2, stdout_cpy;
  char buf[100];

  // check invalid arg
  if (dup(87) != -1) {
    error("dup_test: able to duplicated a non open file");
  }

  // test read only funcionality
  fd1 = open("/small.txt", O_RDONLY);
  if (fd1 < 0) {
    error("dup_test: unable to open small file");
  }

  if ((fd2 = dup(fd1)) != fd1 + 1) {
    error("dup_test: returned fd from dup was not the smallest free fd, was '%d'", fd2);
  }

  // test offsets are respected in dupped files
  assert(read(fd1, buf, 10) == 10);
  
  buf[10] = 0;
  if (strcmp(buf, "aaaaaaaaaa") != 0) {
    error("dup_test: couldn't read from original fd after dup");
  }

  if (read(fd2, buf, 10) != 10) {
    error("dup_test: coudn't read from the dupped fd");
  }

  buf[10] = 0;
  if (strcmp(buf, "aaaaaaaaaa") == 0) {
    error("dup_test: the duped fd didn't respect the read offset from the other file.");
  }

  if (strcmp(buf, "bbbbbbbbbb") != 0) {
    error("dup_test: the duped fd didn't read the correct 10 bytes at the 10 byte offset");
  }

  if (close(fd1) != 0) {
    error("dup_test: fail to close the original file");
  }

  if (read(fd2, buf, 5) != 5) {
    error("dup_test: wasn't able to read from the duped file after the original file was closed");
  }

  buf[5] = 0;
  assert(strcmp(buf, "ccccc") == 0);

  if (close(fd2) != 0) {
    error("dup_test: fail to close the duped file");
  }

  // test duping of stdout
  // should be fd1, because that is the first file I opened (and closed) earlier
  if ((stdout_cpy = dup(stdout)) != fd1) {
    error("dup_test: returned fd from dup that was not the smallest free fd, was '%d'", stdout_cpy);
  }

  char *consolestr = "print to console directly from write ";
  strcpy(buf, consolestr);

  if (write(stdout_cpy, consolestr, strlen(consolestr)) != strlen(consolestr)) {
    error("dup_test: couldn't write to console from duped fd");
  }
  assert(close(stdout_cpy) == 0);
  pass("");
}
