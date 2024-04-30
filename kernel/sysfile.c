//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <cdefs.h>
#include <defs.h>
#include <fcntl.h>
#include <file.h>
#include <fs.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <sleeplock.h>
#include <spinlock.h>
#include <stat.h>

// Validates the file descriptor, returning -1 on a fail
// and 0 on a success. Output parameter retfd
// is only valid on success and holds the found file descriptor.
static int argfd(int argnum, int* retfd);


int sys_dup(void) {
  // LAB1
  int fd; 

  // Checks that fd is within valid bounds
  if(argfd(0, &fd) == -1)
    return -1;

  return fdup(fd);
}

int sys_read(void) {
  char* buf;
  int fd, left_to_read;

  // Get the fd, buffer, and the amount to read
  // while making sure they are within valid ranges
  argint(2, &left_to_read);
  if(argptr(1, &buf, left_to_read) == -1
     || argfd(0, &fd) == -1)
    return -1;
  
  return fread(fd, buf, left_to_read);
}

int sys_write(void) {
  // you have to change the code in this function.
  // Currently it supports printing one character to the screen.

  char* buf;
  int fd, left_to_write;

  // Get the fd, buffer, and the amount to read
  // while makng sure that their ranges are valid
  argint(2, &left_to_write);
  if(argptr(1, &buf, left_to_write) == -1
     || argfd(0, &fd) == -1)
    return -1;
 
  return fwrite(fd, buf, left_to_write);
}

int sys_close(void) {
  // LAB1
  
  // Make sure that the given fd is within a valid range
  int fd;
  if(argfd(0, &fd) == -1)
    return -1;

  return fclose(fd);
}

int sys_fstat(void) {
  // LAB1

  struct stat* file_stat;
  int fd;


  // Get the fd and stat buffer while making sure they are
  // within valid ranges
  if(argptr(1, (char**) &file_stat, sizeof(struct stat)) == -1
     || argfd(0, &fd) == -1)
    return -1;

  return fstat(fd, file_stat);
}

int sys_open(void) {
  // LAB1
  
  char* path;
  int mode;

  // Verify that arguments are valid.
    // a. Check that path arg actually points to a file, and retrieve inode struct if it does
    // b. Check that we have a valid mode
    // If anything is wrong return -1

  if(argstr(0, &path) == - 1 || argint(1, &mode) == -1)
    return -1;  
  
  return fopen(path, mode);
}

int sys_exec(void) {
  //  LAB2

  char* path;
  char** arguments;

  // Get the arguments and path parameters
  if(argptr(0, (char **) &path, sizeof(path)) == -1
     || argptr(1, (char**) &arguments, sizeof(arguments)) == -1)
    return -1;

  // Make sure every  single pointer in argv points to a valid address
  for(int j = 0; arguments[j] != NULL; j++) {
    char* p;
    if(fetchstr((int64_t) arguments[j], &p) == -1)
      return -1; 
  }


  return exec(path, arguments);
}

int sys_pipe(void) {
  
  int* fds;

  // Get the fd and stat buffer while making sure they are
  // within valid ranges
  if(argptr(0, (char **) &fds, sizeof(fds)) == -1)
    return -1;

  
  return fpipe(fds);
}

int sys_unlink(void) {
  // LAB 4
  
  // Call argstr on argument 0 to acquire the path for the function.
  // If argptr returns -1 then return -1
  // Call funlink and return its result.
  char* path;

  if(argstr(0, &path) == - 1)
    return -1;

  return unlink(path);
  
}

static int argfd(int argnum, int* retfd) {

  int fd;
  argint(argnum, &fd);

  // Make sure the fd is within the appropriate range
  if(fd < 0 || fd >= NOFILE)
      return -1;

  // Return the acquired fd and exit with success
  *retfd = fd;
  return 0;
}