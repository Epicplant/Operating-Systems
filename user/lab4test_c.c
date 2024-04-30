#include <cdefs.h>
#include <fcntl.h>
#include <fs.h>
#include <param.h>
#include <stat.h>
#include <syscall.h>
#include <trap.h>
#include <user.h>
#include <test.h>

char buf[8192];
char* file_name = "newfile.txt";
int ROOT_DIR_START_SIZE = 400;
int DIRENT_SIZE = 16;
int INUM_START = 24;

void create_file(int);
void check_system_consistent(bool*);

int main(int argc, char *argv[]) {
  int steps = 0;
  bool created = false;

  if (argc > 1) {
    steps = atoi(argv[1]);
    printf(stdout, "crashing after %d bwrites\n", steps);
  }

  check_system_consistent(&created);
  if (!created) {
    create_file(steps);
    check_system_consistent(&created);
    if (!created) {
      error("File was not created. Not consistent.");
    }
  }

  pass("lab4 part c test!");
  exit();
  return 0;
}

// Tries to create a file.
// Will crash afters steps bwrite calls if (steps != 0).
void create_file(int steps) {
  int fd;
  if (steps) {
    crashn(steps);
  }
  fd = open(file_name, O_CREATE | O_RDWR);
  assert(fd >= 0);
}

// will return only if
// system is consistent.
void check_system_consistent(bool* created) {
  int fd;
  struct stat st;
  assert(created != NULL);

  fd = open(file_name, O_RDONLY);
  if (fd < 0) {
    *created = false;
    // File has not been created yet.
    // Ensure the root directory hasn't changed.
    fd = open(".", O_RDONLY);
    if (fstat(fd, &st) != 0) {
      error("New file does not exist: Cannot stat root directoy. Not consistent.");
    }
    if (st.size > ROOT_DIR_START_SIZE) {
      error("Root dir size grew, but data does not exist. Not consistent.");
    }
    if (st.size < ROOT_DIR_START_SIZE) {
      error("Root size shrunk, uh oh. Not consistent.");
    }
  } else {
    *created = true;
    if (fstat(fd, &st) != 0) {
      error("Cannot stat new file. Not consistent.");
    }
    // Ensure that the inode wasn't already allocated in the inodefile.
    // Note: This will error when the inodefile is updated, but the root inode wasn't.
    if (st.ino != INUM_START + 1) {
      error("New file inum is invalid, inum=%d, inum_expected=%d. Not consistent.", st.ino, INUM_START + 1);
    }
    assert(close(fd) == 0);

    // Ensure that the root directory grew by 1 dirent.
    fd = open(".", O_RDONLY);
    if (fstat(fd, &st) != 0) {
      error("Cannot stat root directoy. Not consistent.");
    }
    if (st.size != ROOT_DIR_START_SIZE + DIRENT_SIZE) {
      error("File exists but root directory size is incorrect, size=%d, size_expected=%d. Not consistent.", st.size, ROOT_DIR_START_SIZE + DIRENT_SIZE);
    }
    assert(close(fd) == 0);
  }
}
