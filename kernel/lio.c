// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//
// The implementation uses two state flags internally:
// * B_VALID: the buffer data has been read from the disk.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

#include <cdefs.h>
#include <defs.h>
#include <fs.h>
#include <param.h>
#include <sleeplock.h>
#include <spinlock.h>

#include <buf.h>

static const int NLOGBLK = 79;
struct sleeplock loglock;

static logheader cachedheader;
struct superblock super;

static logheader getlogheader() {
  struct buf* logblock;
  
  // Acquire the log header
  logheader header;
  logblock = bread(ROOTDEV, super.logstart);
  memmove(&header, logblock->data, sizeof(header));
  brelse(logblock);

  return header;
}

static void setlogheader(logheader header) {
  struct buf* logblock;
  
  // Update the log header
  logblock = bread(ROOTDEV, super.logstart);
  memmove(logblock->data, &header, sizeof(header));
  bwrite(logblock);
  brelse(logblock);

  // logheader test;
  // logblock = bread(ROOTDEV, super.logstart);
  // memmove(&test, logblock->data, sizeof(test));
  // brelse(logblock);
}

static int findlatestheaderidx(uint data[]) {

  for(int i = 0; i < NLOGBLK; i++) {
    if(data[i] == 0) {
      return i;
    }
  }

  return -1;
}

// To begin a transaction. Should always have a matching logcommit call at the end.
void logbegin() {
  
  // Lock the log header (AKA the log region) and lock it
  acquiresleep(&loglock);

  //struct buf* block;

  // Zero out the log region
  // for(int i = 0; i < NLOGBLK-1; i++) {
  //   block = bread(ROOTDEV, super.logstart + i);
  //   memset(block->data, 0, BSIZE);
  //   bwrite(block);
  //   brelse(block);
  // }

  logheader header;

  // Zero out the log header to start from default state
  memset(&header, 0, sizeof(header));

  // Set the committed field to be -1
  header.commit = 0;

  cachedheader = header;

  // No point in writing header to disk yet, since we have nothing in
  // in the log file yet
  setlogheader(header);
}

// To finish a transaction. Indicates to the journalling layer that all necessary
// operations have been noted and can be flushed to disk. If this function
// completes then client can be assured that operation will be reflected on disk.
void logcommit() {

  struct buf* logblock;
  struct buf* extentblock;

  // Say the data we have batched is ready to be committed
  cachedheader.commit = 1;
  setlogheader(cachedheader);

  // Iterate through the log region and begin
  // transferring all blocks stored in the log region (the 79
  // blocks after the logheader block) to the locations they
  // were intended to be in memory (stored in the data field)
  for(int i = 0; i < NLOGBLK; i++) {
    

    if(cachedheader.data[i] == 0) {
      break;
    }    

    // Acquire the log block
    logblock = bread(ROOTDEV, super.logstart + 1 + i);

    // struct dinode test1;
    // memmove(&test1, logblock->data, sizeof(struct dinode));

    // struct dirent test2;
    // for(int j = 0; j < 500; j += sizeof(struct dirent)) {
    //   memmove(&test2, logblock->data + j, sizeof(struct dirent));
    //   int stop = 0;
    // }

    // Acquire the actual location block
    extentblock = bread(ROOTDEV, cachedheader.data[i]);
    
    // Move the data from the log block to the extent block
    memmove(extentblock->data, logblock->data, BSIZE);
    
    bwrite(extentblock);

    // Release the two buffers
    brelse(logblock);
    brelse(extentblock);
  }

  // Finished committing everything
  memset(&cachedheader, 0, sizeof(cachedheader));
  setlogheader(cachedheader);

  // Unlock the log region and update it in disk
  releasesleep(&loglock);
}

// To indicate a buffer write which should occur atomically as part of the
// current transaction. Logs the write to the log. Must be called while in a transaction.
int logwrite(struct buf* block, uint location) {
    
  // If the committed field is 1, then that means we are writing 
  // to the log after it a previous one has been committed and before
  // a new one is begun. As a result, return -1.
  if(cachedheader.commit != 0) {
    return -1;
  }

  // If we no longer have any available blocks within the log region
  // (the last index in the data array is zero), then we return 0 to indicated the
  // log must be committed and begun again before we can write anymore
  if(cachedheader.data[NLOGBLK-1] != 0) {
    logcommit();
    logbegin();
  }

  // Instead of writing the block to its location on disk, we want to
  // Update the log header with the locations that the blocks were
  // originally being written to in disk. Based on the index of the
  // of the data field we place the location, we move to sb.logstart + i
  // to get the sector within the log region where we will
  // store the block's data 
  int index = findlatestheaderidx(cachedheader.data);
  struct buf* logblock = bread(ROOTDEV, super.logstart + 1 + index);
  memmove(logblock->data, block->data, BSIZE);
  bwrite(logblock);
  brelse(logblock);

  // block will be brelsed in fs.c
  
  // Update the header's data field
  cachedheader.data[index] = location;

  // Update the header file on disk in case of a crash
  setlogheader(cachedheader);
  
  // return 1 on success
  return 1;
}

// Upon reload, check to see if commit occurred or not.
// If a commit was in the process, then move everything from log file
void logapply() {

  // Init the log header's sleeplock in case this is the first time
    initsleeplock(&loglock, "log header");

    acquiresleep(&loglock);

    readsb(ROOTDEV, &super);
    // Get the log header from disk
    cachedheader = getlogheader();

    // If the committed field is not -1, then that means we weren't
    // in the process of logging
    if(cachedheader.commit == 0) {
      releasesleep(&loglock);
      return;
    }

    // Call logcommit to dump the log region.
    logcommit();
}