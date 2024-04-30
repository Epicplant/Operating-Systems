#pragma once

#include <extent.h>
#include <sleeplock.h>

// The in-memory inode structure. Every file within the `xk`
// filesystem is represented by an in-memory inode. All `struct inode`s
// correspond to an on-disk inode structure. For more info see `struct dinode`
// in `fs.h`.
struct inode {
  // Device number of the inode. Each hard drive attached to the computer has a
  // device number. Only meaningful for inodes that represent on-disk files.
  uint dev;
  // The inode number. Every inode has a unique inum. The inum is the index of
  // the inode within the inodefile. Thus INODEOFF(inum) gives the offset into
  // the inodefile at which the on-disk inode's data is located.
  uint inum;
  // The number of in-memory references to this in-memory `struct inode`.
  int ref;
  // Tracks whether or not the inode is valid. 1 if valid, 0 otherwise. A
  // `struct inode` is valid once it has been populated from disk using `locki`.
  int valid;
  // The lock for this inode. Should only be used after initialized by `iinit`.
  struct sleeplock lock;

  // Copy of information from the corresponding disk inode (see `struct dinode`
  // in fs.h for details).
  short type; 
  short devid;
  uint size;
  struct extent data[30];
};

// table mapping device ID (devid) to device functions
struct devsw {
  int (*read)(struct inode *, char *, int);
  int (*write)(struct inode *, char *, int);
};

extern struct devsw devsw[];
// Device ids
enum {
  CONSOLE = 1,
};

int fdup(int fd);
int fread(int fd, char* buf, int left_to_read);
int fwrite(int fd, char* buf, int left_to_write);
int fclose(int fd);
int fstat(int fd, struct stat* file_stat);
int fopen(char* path, int mode);
int fpipe(int* fds);


// Pipe buffer
struct p_buf {
  int wr_offset;
  int rd_offset;
  bool read_open;
  bool write_open;
  int buffer_size;
  int curr_writer_pid;
  int size;
  struct spinlock lock;
  char* pipe_buf;
} typedef p_buf;

// The info of a file being I/O'd.
// Node represents the files inode (AKA which file this is).
// Offset represents the position in the file from the beginning
// Mode is the files I/O mode (0 for read-only, 1 for write-only, 2 for read-write)
// Reference is the number of times this file is accessed at a time
struct file_info {
  struct inode* node;
  int offset;
  int mode;
  int reference;
  struct sleeplock lock;
  p_buf* buffer;
} typedef file_info;

extern file_info infos[];
