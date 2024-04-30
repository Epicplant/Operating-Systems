// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xk/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.


#include <cdefs.h>
#include <defs.h>
#include <file.h>
#include <fs.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <sleeplock.h>
#include <spinlock.h>
#include <stat.h>
#include <fcntl.h>
#include <buf.h>


// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb;


// Read the super block.
void readsb(int dev, struct superblock *sb) {
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Find the offset of the first empty position in the root directory for a dirent
// and adds the parameter dirent there. Returns -1 on failure and the offset in
// the directory on success
static int findemptydirentoffset();

// Find the offset of the first empty position in the inodefile for a dinode
// and adds the parameter dinode there. Returns -1 on failure and the offset
// in the file on success
static int 
findemptyinodeoffset(struct dinode* di);

// Write to an inode's file at a given offset using the src arrays data
// If the offset extends beyond currently allocated space for
// the file then balloc new blocks for the file
static int writetoextent(struct inode* node, char* src, int off, int n);

// Read from an inode's file data at a given offset and places
// the results in the dst array
static int readfromextent(struct inode* node, char* dst, int off, int n);

/*
 * arg0: char * [path to the file]
 * 
 * Given a pathname for a file, if no process has an open reference to the
 * file, sys_unlink() removes the file from the file system.
 *
 * On success, returns 0. On error, returns -1.
 *
 * Errors:
 * arg0 points to an invalid or unmapped address
 * there is an invalid address before the end of the string
 * the file does not exist
 * the path represents a directory or device
 * the file currently has an open reference
 */
int unlink(char*);


// mark [start, end] bit in bp->data to 1 if used is true, else 0
static void bmark(struct buf *bp, uint start, uint end, bool used)
{
  int m, bi;
  for (bi = start; bi <= end; bi++) {
    m = 1 << (bi % 8);
    if (used) {
      bp->data[bi/8] |= m;  // Mark block in use.
    } else {
      if((bp->data[bi/8] & m) == 0)
        panic("freeing free block");
      bp->data[bi/8] &= ~m; // Mark block as free.
    }
  }
  bp->flags |= B_DIRTY; // mark our update
}

// Blocks.

// Allocate n disk blocks, no promise on content of allocated disk blocks
// Returns the beginning block number of a consecutive chunk of n blocks
// __attribute__((unused)) suppresses unused warning, can be removed once this
// function is called in lab 4.
static uint balloc(uint dev, uint n)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for (b = 0; b < sb.size; b += BPB) {
    bp = bread(dev, BBLOCK(b, sb)); // look through each bitmap sector

    uint sz = 0;
    uint i = 0;
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
      m = 1 << (bi % 8);
      if ((bp->data[bi/8] & m) == 0) {  // Is block free?
        sz++;
        if (sz == 1) // reset starting blk
          i = bi;
        if (sz == n) { // found n blks
          bmark(bp, i, bi, true); // mark data block as used
          
          // flush the buffer to disk
          bwrite(bp);
          brelse(bp);
          return b+i;
        }
      } else { // reset search
        sz = 0;
        i = 0;
      }
    }
    brelse(bp);
  }
  panic("balloc: can't allocate contiguous blocks");
}

// Free n disk blocks starting from b.
// __attribute__((unused)) suppresses unused warning, can be removed once this
// function is called in lab 4.
static void bfree(int dev, uint b, uint n)
{
  struct buf *bp;

  assertm(n >= 1, "freeing less than 1 block");
  assertm(BBLOCK(b, sb) == BBLOCK(b+n-1, sb), "returned blocks live in different bitmap sectors");

  bp = bread(dev, BBLOCK(b, sb));
  bmark(bp, b % BPB, (b+n-1) % BPB, false);
  bwrite(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// range of blocks holding the file's content.
//
// The inodes themselves are contained in a file known as the
// inodefile. This allows the number of inodes to grow dynamically
// appending to the end of the inode file. The inodefile has an
// inum of 0 and starts at sb.startinode.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->flags.
//
// Since there is no writing to the file system there is no need
// for the callers to worry about coherence between the disk
// and the in memory copy, although that will become important
// if writing to the disk is introduced.
//
// Clients use iget() to populate an inode with valid information
// from the disk. idup() can be used to add an in memory reference
// to and inode. irelease() will decrement the in memory reference count
// and will free the inode if there are no more references to it,
// freeing up space in the cache for the inode to be used again.



struct {
  struct spinlock lock;
  struct inode inode[NINODE];
  struct inode inodefile;
  struct sleeplock openlock;
} icache;

// Find the inode file on the disk and load it into memory
// should only be called once, but is idempotent.
static void init_inodefile(int dev) {
  struct buf *b;
  struct dinode di;

  b = bread(dev, sb.inodestart);
  memmove(&di, b->data, sizeof(struct dinode));

  icache.inodefile.inum = INODEFILEINO;
  icache.inodefile.dev = dev;
  icache.inodefile.type = di.type;
  icache.inodefile.valid = 1;
  icache.inodefile.ref = 1;

  icache.inodefile.devid = di.devid;
  icache.inodefile.size = di.size;
  for(int i = 0; i < 30; i++) {
    icache.inodefile.data[i] = di.data[i];
  }

  brelse(b);
}

void iinit(int dev) {
  int i;

  initlock(&icache.lock, "icache");
  for (i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }
  initsleeplock(&icache.inodefile.lock, "inodefile");

  readsb(dev, &sb);
  cprintf("sb: size %d nblocks %d bmap start %d inodestart %d\n", sb.size,
          sb.nblocks, sb.bmapstart, sb.inodestart);

  init_inodefile(dev);
}


// Reads the dinode with the passed inum from the inode file.
// Threadsafe, will acquire sleeplock on inodefile inode if not held.
static void read_dinode(uint inum, struct dinode *dip) {
  int holding_inodefile_lock = holdingsleep(&icache.inodefile.lock);
  if (!holding_inodefile_lock)
    locki(&icache.inodefile);

  readi(&icache.inodefile, (char *)dip, INODEOFF(inum), sizeof(*dip));

  if (!holding_inodefile_lock)
    unlocki(&icache.inodefile);

}

static void write_dinode(uint inum, struct dinode *dip) {
  int holding_inodefile_lock = holdingsleep(&icache.inodefile.lock);
  if (!holding_inodefile_lock)
    locki(&icache.inodefile);

  writei(&icache.inodefile, (char *)dip, INODEOFF(inum), sizeof(*dip));

 
  if (!holding_inodefile_lock)
    unlocki(&icache.inodefile);
}


// Find the inode with number inum on device dev
// and return the in-memory copy. Does not read
// the inode from from disk.
static struct inode *iget(uint dev, uint inum) {
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if (empty == 0 && ip->ref == 0) // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if (empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->ref = 1;
  ip->valid = 0;
  ip->dev = dev;
  ip->inum = inum;

  release(&icache.lock);

  return ip;
}

// looks up a path, if valid, populate its inode struct
struct inode *iopen(char *path, int mode) {
  
  acquiresleep(&icache.openlock);
  
  struct inode* inode = namei(path);
  if (inode != NULL) {
    locki(inode);
    unlocki(inode);
  } else if(O_CREATE == (O_CREATE & mode)) {  // The file does not already exist
   
    
    // Construct a struct dinode called din
    struct dinode di;
    // Update the din with the correct information.
    // Type will always be that of file, size will be that of 0,
    // the devid will be the DEVROOT constant (since we only have 1 drive),
    // data will not be modified since it starts empty, and
    // pad will likewise not be modified.
    di.type = T_FILE;
    di.devid = ROOTDEV;
    di.size = 0;
    memset(di.data, 0, sizeof(struct extent) * 30);

    // Iterate through the inodefile until we find a position to add our new files dinode data
    int inodepos = findemptyinodeoffset(&di);
    if(inodepos == -1) {
      //releasesleep(&icache.openlock);
      return NULL;  
    }

    // Add new file to the free position in the inodefile
    write_dinode(inodepos / sizeof(struct dinode), &di);
 
    // Create the dirent we are adding to the rootdir
    struct dirent entry;
    memset(&entry, 0, sizeof(struct dirent));
    int size = strlen(path);
    for(int i = 0; i < size && i < DIRSIZ; i++) {
      entry.name[i] = path[i];
    }
    entry.name[size+1] = '\0';
    entry.inum = inodepos / sizeof(struct dinode);

    // Acquire the rootdir, concurrent_writei onto the end of it
    // a new dirent containing the data of the file
    struct inode* rootdir = iget(ROOTDEV, ROOTINO);

    int rootpos = findemptydirentoffset();
    if(rootpos == -1) {
      irelease(rootdir);
      //releasesleep(&icache.openlock);
      return NULL;
    }

    concurrent_writei(rootdir, (char*) &entry, rootpos, sizeof(struct dirent));

    irelease(rootdir);

    // Acquire/create the inode we will be using for this file 
    struct inode* returner = iget(ROOTDEV,  inodepos / sizeof(struct dinode));
    

    releasesleep(&icache.openlock);

    return returner;
  }

  releasesleep(&icache.openlock);
  return inode;
}


static int 
findemptyinodeoffset(struct dinode* di) {
   
  locki(&icache.inodefile);
  struct dinode node;
  int offset = 0;

  // Iterate through every data field
  for(int i = 0; i < 30; i++) {
    
    // Ran out of space in the inode, allocate space onto the end of the file and put thing there
    if(icache.inodefile.data[i].nblocks == 0) {

      unlocki(&icache.inodefile);
      return icache.inodefile.size;     
    }

    // For each extent's blocks
    for(int j = 0; j < icache.inodefile.data[i].nblocks; j++) {
      
      
       // Within the given inodes device, begin reading at the extent the
        // offset specifies
        struct buf* bp = bread(icache.inodefile.dev, icache.inodefile.data[i].startblkno + j);

        for(int k = 0; k < BSIZE; k += sizeof(node)) {
          
          
          memmove(&node, bp->data + k, sizeof(node));
          if(node.type == -1) {
          
            brelse(bp);
            unlocki(&icache.inodefile);
            return offset;
            
          }

          offset += sizeof(node);
        }

        brelse(bp);
    }
  }

  // We already have 30 extents
  unlocki(&icache.inodefile);
  return -1;
}

static int findemptydirentoffset() {
  struct inode* rootdir = iget(ROOTDEV, 1);
  struct dirent entry;
  int offset = 0;

  locki(rootdir);

  // Iterate through every data field
  for(int i = 0; i < 30; i++) {
    
    // Ran out of space in the inode, allocate space onto the end of the file and put thing there
    if(icache.inodefile.data[i].nblocks == 0) {
      unlocki(rootdir);
      irelease(rootdir);
      return rootdir->size;     
    }

    // For each extent's blocks
    for(int j = 0; j < rootdir->data[i].nblocks; j++) {
      
      
       // Within the given inodes device, begin reading at the extent the
        // offset specifies
        struct buf* bp = bread(rootdir->dev, rootdir->data[i].startblkno + j);

        for(int k = 0; k < BSIZE; k += sizeof(entry)) {
          
          memmove(&entry, bp->data + k, sizeof(entry));
          if(entry.inum == 0) {
          
            brelse(bp);
            unlocki(rootdir);
            irelease(rootdir);
            return offset;
            
          }

          offset += sizeof(entry);
        }

        brelse(bp);
    }
  }
  unlocki(rootdir);
  irelease(rootdir);

  // We already have 30 extents
  return -1;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *idup(struct inode *ip) {
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
void irelease(struct inode *ip) {
  acquire(&icache.lock);
  // inode has no other references release
  if (ip->ref == 1)
    ip->type = 0;
  ip->ref--;
  release(&icache.lock);
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void locki(struct inode *ip) {
  struct dinode dip;

  if(ip == 0 || ip->ref < 1)
    panic("locki");

  acquiresleep(&ip->lock);

  if (ip->valid == 0) {

    if (ip != &icache.inodefile)
      locki(&icache.inodefile);
    read_dinode(ip->inum, &dip);
    if (ip != &icache.inodefile)
      unlocki(&icache.inodefile);

    ip->type = dip.type;
    ip->devid = dip.devid;

    ip->size = dip.size; 
    memmove(ip->data, &dip.data, 30 * sizeof(struct extent));
    ip->valid = 1;

    if (ip->type == 0) {
      panic("iget: no type");
    }
  }
}

int unlink(char* path) {


  // Acquire the root directory
  struct inode* rootdir = iget(ROOTDEV, 1);

  // Acquire the offset and the inode of the file in the root directory
  // if it exists
  uint off;
  struct inode* node;
  locki(rootdir);
  node = dirlookup(rootdir, path, &off);
  unlocki(rootdir);
  
  if(node == NULL) {
    irelease(rootdir);
    return -1;
  }
  
  if(node->ref != 1) {
    irelease(rootdir);
    irelease(node);
    return -1;
  }

  // Check that the path represents a directory or a device, and if so return -1
  if(node->type == T_DEV || node->type == T_DIR) {
    irelease(node);
    irelease(rootdir);
    return -1;
  }

  // Free extents of the file
  //locki(node);
  for(int i = 0; i < 30; i++) {
    if(node->data[i].nblocks != 0) {
      bfree(ROOTDEV, node->data[i].startblkno, node->data[i].nblocks);
    }
  }
  //unlocki(node);


  // Remove the inode from the inodefile;
  // Acquire the inode’s dinode so that we can free it
  struct dinode di; 
  di.type = -1;
  // Set the inode's dinode to be a size of -1
  write_dinode(node->inum, &di);
   
  // Remove the file's dirent from the root directory

  struct dirent entry;
  entry.inum = 0;
  if(concurrent_writei(rootdir, (char*) &entry, off, sizeof(entry)) == -1)
    return -1;

  // update rootdir's size in its dinode
  struct inode* rootinode = iget(ROOTDEV, ROOTINO);
  struct dinode rootnode;

  rootnode.size = rootinode->size - sizeof(struct dirent);
  memmove(rootnode.data, rootinode->data, 30 * sizeof(struct extent));
  rootnode.devid = rootinode->devid;
  rootnode.type = rootinode->type;

  write_dinode(rootdir->inum, &rootnode);

  // update the inodefile's size in its dinode;
  
  
  struct dinode inodefilenode;

  inodefilenode.size = icache.inodefile.size - sizeof(struct dirent);
  memmove(inodefilenode.data, icache.inodefile.data, 30 * sizeof(struct extent));
  inodefilenode.devid = icache.inodefile.devid;
  inodefilenode.type = icache.inodefile.type;

  write_dinode(0, &inodefilenode);

  
  // Decrement reference count back to 0 (it gets set to 1 by dirlookup)
  irelease(node);
  irelease(rootdir);

  // Return success
  return 0;
}

// Unlock the given inode.
void unlocki(struct inode *ip) {
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("unlocki");

  releasesleep(&ip->lock);
}

// threadsafe stati.
void concurrent_stati(struct inode *ip, struct stat *st) {
  locki(ip);
  stati(ip, st);
  unlocki(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void stati(struct inode *ip, struct stat *st) {
  if (!holdingsleep(&ip->lock))
    panic("not holding lock");

  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->size = ip->size;
}

// threadsafe readi.
int concurrent_readi(struct inode *ip, char *dst, uint off, uint n) {
  int retval;

  locki(ip);
  retval = readi(ip, dst, off, n);
  unlocki(ip);

  return retval;
}

// Read data from inode.
// Returns number of bytes read.
// Caller must hold ip->lock.
int readi(struct inode *ip, char *dst, uint off, uint n) {
  
  // Acquires the sleeplock
  if (!holdingsleep(&ip->lock))
    panic("not holding lock");

  // Checks if we are a device and returns with a device read if it is valid
  if (ip->type == T_DEV) {
    if (ip->devid < 0 || ip->devid >= NDEV || !devsw[ip->devid].read)
      return -1;
    return devsw[ip->devid].read(ip, dst, n);
  }

  // If the offset is greater than the actual file size or
  // n is negative, we return failure
  if (off > ip->size || off + n < off)
    return -1;

  // If we are reading beyond  the end of the file, cap the amount
  // we read to be the actual space that remains beyond the offset
  if (off + n > ip->size)
    n = ip->size - off;

  int val = readfromextent(ip, dst, off, n);
  if(val == -1) {
    return -1;
  }

  return n;
}

// threadsafe writei.
int concurrent_writei(struct inode *ip, char *src, uint off, uint n) {
  int retval;

  locki(ip);
  retval = writei(ip, src, off, n);
  unlocki(ip);
   
  return retval;
}


// Write data to inode.
// Returns number of bytes written.
// Caller must hold ip->lock.
int writei(struct inode *ip, char *src, uint off, uint n) {
  if (!holdingsleep(&ip->lock))
    panic("not holding lock");

  if (ip->type == T_DEV) {
    if (ip->devid < 0 || ip->devid >= NDEV || !devsw[ip->devid].write)
      return -1;
    return devsw[ip->devid].write(ip, src, n);
  }


  if(n <= 0) {
    return -1;
  }

  // The index denoting which extent we are on
  int blocknum = writetoextent(ip, src, off, n);
  if(blocknum == -1) {
    return -1;
  }

  // increase the size of the file
  if(off + n > ip->size) {
    ip->size += ((off + n) - ip->size); 
    struct dinode di;
    memmove(di.data, ip->data, 30 * sizeof(struct extent));
    di.devid = ip->devid;
    di.size = ip->size;
    di.type = ip->type;
    write_dinode(ip->inum, &di);
  }

  return n;
}

static int 
writetoextent(struct inode* node, char* src, int off, int n) {
  
  bool writing = false;
  int totalsize = 0;
  int totalwritten = 0;
  

  // Iterate through every data field
  for(int i = 0; i < 30; i++) {
    
    // If an extent doesn't already exist here, then we make one and have
    // it be big to extend the file enough to reach the offset
    if(node->data[i].nblocks == 0) {
    
      // Calculate number of blocks we need
      int blocks;
      if(n % BSIZE == 0) {
        blocks = n / BSIZE;
      } else {
        blocks = n / BSIZE + 1;
      }

      // Allocate new space
      node->data[i].startblkno = balloc(node->dev, blocks);
      node->data[i].nblocks = blocks;
      
      // Reset the offset we are at in the blocks to the new position
      off -= totalsize;
      totalsize = 0;
    }
    
    // Skip this extents blocks
    if(off > totalsize + (node->data[i].nblocks * BSIZE)){
      totalsize += node->data[i].nblocks * BSIZE;
      continue;
    }

    int blocksdeep = 0;
    blocksdeep = (off - totalsize) / BSIZE;
    totalsize += (blocksdeep * BSIZE);

    // For each extent's blocks
    for(int j = blocksdeep; j < node->data[i].nblocks; j++) {
      
      // Acknowledge we visited this block
      totalsize += BSIZE;

      // We've reached the block the offset is in, so now we start
      // writing n data
      if(totalsize > off) {
        writing = true;
      }

      // Write data to the current block
      if(writing) {
        
        // Within the given inodes device, begin reading at the extent the
        // offset specifies
        struct buf* bp = bread(node->dev, node->data[i].startblkno + j);
        
        // Writes us to the next block
        int m = min(n, BSIZE - (off % BSIZE));

        // move where we are at in a given block
        // we are writing to
        memset(bp->data + off % BSIZE, 0, m);
        memmove(bp->data + off % BSIZE, src, m);
        bwrite(bp);
        brelse(bp);

        // Increment offset in write's buffer
        src += m;

        // Used to determine how much we can write based on how much we have written already
        totalwritten += m;
        
        // Decrement how much we have left to write from what we
        n -= m;

        // Where we are at in a given block
        off += m;
      }

      if(n == 0) {

        return 0;
      }
    }
  
  }

  // We already have 30 extents
  return -1;
}


static int readfromextent(struct inode* node, char* dst, int off, int n) {

  bool reading = false;
  int totalsize = 0;
  int totalwritten = 0;
  

  // Iterate through every data field
  for(int i = 0; i < 30; i++) {
    
    // We are reading beyond file bounds, return error
    if(node->data[i].nblocks == 0) {
        return -1;
    }

    // Skip this extents blocks
    if(off > totalsize + (node->data[i].nblocks * BSIZE)){
      totalsize += node->data[i].nblocks * BSIZE;
      continue;
    }

    int blocksdeep = 0;
    blocksdeep = (off - totalsize) / BSIZE;
    totalsize += (blocksdeep * BSIZE);

    // For each extent's blocks
    for(int j = blocksdeep; j < node->data[i].nblocks; j++) {
    

      // Acknowledge we visited this block
      totalsize += BSIZE;

      // We've reached the block the offset is in, so now we start
      // writing n data
      if(totalsize > off) {
        reading = true;
      }

      // Write data to the current block
      if(reading) {
        
        // Within the given inodes device, begin reading at the extent the
        // offset specifies
        struct buf* bp = bread(node->dev, node->data[i].startblkno + j);

        // Writes us to the next block
        int m = min(n, BSIZE - (off % BSIZE));

        // move where we are at in a given block
        // we are writing to
        memset(dst, 0, m);
        memmove(dst, bp->data + off % BSIZE, m);
        brelse(bp);

        // Increment offset in write's buffer
        dst += m;

        // Used to determine how much we can write based on how much we have written already
        totalwritten += m;
        
        // Decrement how much we have left to write from what we
        n -= m;

        // Where we are at in a given block
        off += m;
      }

      if(n == 0) {

        return 0;
      }
    }
  
  }

  // We already have 30 extents
  return -1;

}
// Directories

int namecmp(const char *s, const char *t) { return strncmp(s, t, DIRSIZ); }

struct inode *rootlookup(char *name) {
  return dirlookup(namei("/"), name, 0);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode *dirlookup(struct inode *dp, char *name, uint *poff) {
  uint off, inum;
  struct dirent de;

  if (dp->type != T_DIR)
    panic("dirlookup not DIR");

  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if (de.inum == 0)
      continue;
    if (namecmp(name, de.name) == 0) {
      // entry matches path element
      if (poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *skipelem(char *path, char *name) {
  char *s;
  int len;

  while (*path == '/')
    path++;
  if (*path == 0)
    return 0;
  s = path;
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;
  if (len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while (*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name. Returns NULL if not found
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode *namex(char *path, int nameiparent, char *name) {
  struct inode *ip, *next;

  if (*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(namei("/"));

  while ((path = skipelem(path, name)) != 0) {
    locki(ip);
    if (ip->type != T_DIR) {
      unlocki(ip);
      goto notfound;
    }

    // Stop one level early.
    if (nameiparent && *path == '\0') {
      unlocki(ip);
      return ip;
    }

    if ((next = dirlookup(ip, name, 0)) == 0) {
      unlocki(ip);
      goto notfound;
    }

    unlocki(ip);
    irelease(ip);
    ip = next;
  }
  if (nameiparent)
    goto notfound;

  return ip;

notfound:
  irelease(ip);
  return 0;
}


/*
See namex
*/
struct inode *namei(char *path) {
  char name[DIRSIZ];
  return namex(path, 0, name);
}

/*
See namex
*/
struct inode *nameiparent(char *path, char *name) {
  return namex(path, 1, name);
}

