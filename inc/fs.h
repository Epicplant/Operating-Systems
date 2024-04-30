#pragma once

#include "extent.h"

// On-disk file system format.
// Both the kernel and user programs use this header file.

#define INODEFILEINO 0 // inode file inum
#define ROOTINO 1      // root i-number
#define BSIZE 512      // block size

struct logheader {
    int commit;
    uint data[79];
    char padding[512 - ((sizeof(uint) * 79) + sizeof(int))];
} typedef logheader;

// Disk layout:
// [ boot block | super block | free bit map | inode file | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint size;       // Size of file system image (blocks)
  uint nblocks;    // Number of data blocks
  uint bmapstart;  // Block number of first free map block
  uint logstart;   // Block number of the log header
  uint inodestart; // Block number of the start of inode file
  
};

// On-disk inode structure which tracks all necessary information which defines
// a file. The inodefile is an array of `struct dinode`s
// where the inode with inum `i` starts at file offset INODEOFF(i).
//
// NOTE(!): For atomicity purposes you must ensure that `sizeof(struct dinode)`
// is a power of 2 and is <= BSIZE. (This ensures that it's always possible to
// contiguously lay out `struct dinode`'s such that none ever span more than one
// disk block).
struct dinode {
  // File type (device, directory, regular file). See stat.h for available types.
  short type;
  // The device number (NOTE: only relevant for device files, i.e.: files with type T_DEV).
  // e.g.: the console might have device number 1, and that's used to distinguish what
  // read and write handlers should be used for all device files with device number 1. 
  short devid;
  // The size of the file in bytes.
  uint size;
  // The extent for the file's data. See `struct extent` in `extent.h` for more info.
  struct extent data[30];
  // We pad the struct such that `sizeof(struct dinode)` is a power of 2.
  char pad[8];
};

// offset of inode in inodefile
#define INODEOFF(inum) ((inum) * sizeof(struct dinode))

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + (sb).bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};
