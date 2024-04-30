//
// File descriptors
//

#include <cdefs.h>
#include <defs.h>
#include <file.h>
#include <fs.h>
#include <fcntl.h>
#include <param.h>
#include <stat.h>
#include <sleeplock.h>
#include <spinlock.h>
#include <proc.h>

#define MAX(X, Y) (((X) < (Y)) ? (Y) : (X))

struct devsw devsw[NDEV];

struct {
    file_info infos[NFILE];
    struct spinlock global_lock;
} filetable;


// Finds an fd in the process list where a file_info* can be added.
// Returns a -1 on failure and the index of insertion
// within the process list on success. Parameter process is the
// current process we are on and fd is the fd of what we are adding
static int add_process_file(struct proc* process, file_info* fd);

// Finds an fd in the process list where a file_info* can be added.
// Returns a -1 on a failure and the index of insertion within the
// global list on a success. Paraemter info is the 
// file info of what we are adding.
static int add_global_file(file_info info);

// Performs a write to a pipe buffer. The fd points to the write
// end of a pipe, buf is the characters we are writing into it, and
// left_to_write is the amount of data we are ultimately writing into
// the buffer. Partial writes are not allowed and will continuously wait
// on reads until there is enough space to complete the write. If either
// the write or the read end are closed we return a failure as in both
// cases there is no point to writing. We return the amount we write
// overall (not just to the buffer at any given time) in the general case. Assumes
// that the file being read to has already had its sleep lock be acquired.
static int pipewrite(int fd, char* buf, int left_to_write);

// Performs a read from a pipe buffer. The fd points to the read
// end of the pipe, buf is what we are holding the characters
// we area reading from the buffer into. Left_to_read is how much
// data we are reading from the buffer. Partial reads are allowed,
// and we may not read as much as left_to_read states if we are reading
// more than the pipe buffer can hold. If the write end of the pipe
// is closed and there is no more data to read in the pipe_buffer, we return
// 0, and we return -1 if the read end of the pipe is closed. We return
// the amount we read overall (anything between 0 and left_to_read) in the
// general case.
static int piperead(int fd, char* buf, int left_to_read);


int fpipe(int* fds) {

  struct proc* process = myproc();

  // Initialize the buffer and its meta data
  p_buf* buffer = (p_buf*) kalloc();
  if(buffer == NULL)
    return -1;

  buffer->buffer_size = 4096-(5 * sizeof(int))-(sizeof(bool) * 2) - sizeof(struct spinlock);
  initlock(&buffer->lock, "pipe");
  buffer->rd_offset = 0;
  buffer->read_open = true;
  buffer->wr_offset = 0;
  buffer->write_open = true;
  buffer->size = 0;
  buffer->pipe_buf = (char*) (buffer+1);


  
  // Open the two  file descriptors  
  int opened1 = fopen(NULL, O_PIPERD);
  int opened2 = fopen(NULL, O_PIPEWR);

  if(opened1 == -1 && opened2 == -1) {
    return -1;
  } else if(opened1 == -1 ) {
    fclose(opened2);
    return -1;
  } else if(opened2 == -1) {
    fclose(opened1);
    return -1;
  }

  // Update the two file_infos to contain the pipe buffer
  process->infos[opened1]->buffer = buffer;
  process->infos[opened2]->buffer = buffer;

  // Set output parameters
  fds[0] = opened1;
  fds[1] = opened2;  

  return 0;
}

int fdup(int fd) {
  // LAB1
  
  // Get current process
  struct proc* process = myproc();
  
  // Check that old_fd is still valid
  if(process->infos[fd] == NULL) {
    return -1;
  }

  acquiresleep(&process->infos[fd]->lock);

  // Add process to file if possible
  int fd1 = add_process_file(process, process->infos[fd]);
  if(fd1 == -1) {
    releasesleep(&process->infos[fd]->lock);
    return -1;
  }
    
  process->infos[fd1] = process->infos[fd];

  // Increment reference count 
  process->infos[fd]->reference++;

  releasesleep(&process->infos[fd]->lock);

  // Didn't find room for the value in the process list so we return -1.
  return fd1;
}

int fread(int fd, char* buf, int left_to_read) {
  int num_read;

  // Get current process
  struct proc* process = myproc();

  if(process->infos[fd] == NULL) {
    return -1;
  }

  if(left_to_read == 0) {
    return 0;
  }

  acquiresleep(&process->infos[fd]->lock);

  // We are writing to the "write end" of a pipe
  if(process->infos[fd]->mode == O_PIPERD) {
  
    releasesleep(&process->infos[fd]->lock);
    return piperead(fd, buf, left_to_read);
  }


  // Check that parameters contents are valid
  if ((process->infos[fd]->mode != O_RDONLY
      && process->infos[fd]->mode != O_RDWR)
      || left_to_read < 0) {

     releasesleep(&process->infos[fd]->lock);
     return -1;
   }
    
  // Reads as much as we can from the file
  if((process->infos[fd]->node->size - process->infos[fd]->offset) < left_to_read) {
    left_to_read = process->infos[fd]->node->size - process->infos[fd]->offset;
  }
  
  // Read as much as possible (num_read) from a given file into buf
  num_read = concurrent_readi(process->infos[fd]->node, buf,
                              process->infos[fd]->offset, left_to_read);

  // If num_read is -1 then an error has likely occurred and we
  // test to see if the value is bad
  if (num_read == -1) {
      releasesleep(&process->infos[fd]->lock);
      return -1;
  }

  // Null terminate the end of the buf
  // buf[num_read] = '\0';

  // Increment the file offset
  process->infos[fd]->offset += num_read;

  releasesleep(&process->infos[fd]->lock);

  // Return number of bytes read
  return num_read;
}


int fwrite(int fd, char* buf, int left_to_write) {
  // you have to change the code in this function.
  // Currently it supports printing one character to the screen.

  int num_written;
  
  // Get current process
  struct proc* process = myproc();
  
  // Check if we are writing to a closed fd
  if(process->infos[fd] == NULL) {
    return -1;
  }

  // We are writing nothing, so return 0
  if(left_to_write == 0) {
    return 0;
  }
  
   acquiresleep(&process->infos[fd]->lock);

  // We are writing to the "write end" of a pipe
  if(process->infos[fd]->mode == O_PIPEWR) {   
    return pipewrite(fd, buf, left_to_write);
  }

  // Ensure paraemter contents are valid
  if((process->infos[fd]->mode != O_WRONLY
     && process->infos[fd]->mode != O_RDWR)
     || left_to_write < 0) {
    
    releasesleep(&process->infos[fd]->lock);
    return -1;
  }


  // Write as much as possible (num_written) from a given buf into a file
  num_written = concurrent_writei(process->infos[fd]->node, buf,
                                  process->infos[fd]->offset, left_to_write);

  // If num_written is -1 then an error has likely occurred and we
  // test to see if the value is bad
  if (num_written == -1) {
      releasesleep(&process->infos[fd]->lock);
      return -1;
  }

  // Null terminate the end of the buf
  // buf[num_written] = '\0';

  // Increment the file offset
  process->infos[fd]->offset += num_written;

  releasesleep(&process->infos[fd]->lock);

  // Return the number of bytes written
  return num_written;
}


int fclose(int fd) {
  // LAB1

  // Get the current process
  struct proc* process = myproc();

  // Checks to see if we are trying to connect to a fd that
  // has never been used or one that has not been opened yet
  if(process->infos[fd] == NULL) {
    return -1;
  }

  acquiresleep(&process->infos[fd]->lock);
 
  // Decrement the  reference count of the global file, then
  // remove it from process table by making it NULL. 
  // This ensures that if we reopen the fd globally
  // in the future we don't have a pointer that should
  // be closed for the process appearing to be "reopened."
  // If global reference count is now 0 then clean up
  // by releasing the inode.
  process->infos[fd]->reference--;
  
  if(process->infos[fd]->reference == 0 && process->infos[fd]->node != NULL)
      irelease(process->infos[fd]->node);

  // We need to close either the read or write end of the buffer
  if(process->infos[fd]->reference == 0 && process->infos[fd]->buffer != NULL) {
    if(process->infos[fd]->mode == O_PIPERD){
      process->infos[fd]->buffer->read_open = false;
    } else {
      process->infos[fd]->buffer->write_open = false;
    }

    // Both ends are closed so free the buffer
    if(!process->infos[fd]->buffer->read_open && !process->infos[fd]->buffer->write_open) {
      kfree((char*) process->infos[fd]->buffer);
    } 
  }
  
  releasesleep(&process->infos[fd]->lock);
  
  process->infos[fd] = NULL;
 

  // Return success
  return 0;
}

int fstat(int fd, struct stat* file_stat) {
  // LAB1

  // Get the current process
  struct proc* process = myproc();

  if(process->infos[fd] == NULL) {
    return -1;
  }

  acquiresleep(&process->infos[fd]->lock);

  // Use the fstat system call to fetch a "struct stat" that describes
  // properties of the file. ("man 2 fstat").
  concurrent_stati(process->infos[fd]->node, file_stat);

  releasesleep(&process->infos[fd]->lock);

  // Return Success
  return 0;
}

int fopen(char* path, int mode) {
  // LAB1
  
  struct file_info fi;
  // Check if we are opening a pipe. If we aren't, we need specific data
  if(mode == O_PIPERD || mode == O_PIPEWR) {
    
    // No actual inode for pipes
    fi.node = NULL;
  } else {
    // Checks that address is valid and returns null if not
    struct inode* nodeptr = iopen(path, mode);
    // Checks that we are in readonly mode and that the file exists/has a valid address
    

    if(nodeptr == NULL)
      return -1;
    fi.node = nodeptr;
  }

  // Create the rest of this files default file info
  if(mode == (O_CREATE | O_RDONLY)) {
    fi.mode = O_RDONLY;
  } else if(mode == (O_CREATE | O_WRONLY)) {
    fi.mode = O_WRONLY;
  } else if(mode == (O_CREATE | O_RDWR)) {
    fi.mode = O_RDWR;
  } else {
    fi.mode = mode;
  }

  fi.offset = 0;
  fi.reference = 1;
  initsleeplock(&fi.lock, "file_info");
  fi.buffer = NULL;

  // Obtain the current process
  struct proc* process = myproc();

  acquire(&filetable.global_lock);

  // Find the process file list fd
  int fd1 = add_global_file(fi);
  if(fd1 == -1) {
    release(&filetable.global_lock);
    return -1;
  }

  int fd2 = add_process_file(process, &filetable.infos[fd1]);
  if(fd2 == -1) {
   release(&filetable.global_lock);
    return -1;
  }
  
  // Add both to their respective lsits
  filetable.infos[fd1] = fi;
  process->infos[fd2] = &filetable.infos[fd1];
  
  release(&filetable.global_lock);
  
  // Add the file info to the appropriate process file list
  return fd2;
}


static int add_global_file(file_info info) {

  // Find an index in our infos list that we can store tha value in
  for(int i = 0; i < NFILE; i++) {
    
    // Check if this value in the global table has no references,
    // if it doesn't have any it means it is safe copy over and is
    // effectively an empty space
    
   // acquiresleep(&filetable.infos[i].lock);
    if(filetable.infos[i].reference == 0) {
      return i;
    }
    //releasesleep(&filetable.infos[i].lock);
  }
  return -1;
}

static int add_process_file(struct proc* process, file_info* info) {

  // Loop through all file_info slots for the associated process
  for(int i = 0; i < NOFILE; i++) {

      // If we find an available slot (either in a spot
      // that has never had an fd or a spot that
      // is no longer being used) then exit with success
      // and finally update everything
      if(process->infos[i] == NULL) {
         return i;
      }
  }

  // All spots in process infos list are full
  return -1;
}



int pipewrite(int fd, char* buf, int left_to_write) {

    struct proc* process = myproc();
    acquire(&process->infos[fd]->buffer->lock); 

    // There will never be enough room to write what we want to write
    if(!process->infos[fd]->buffer->write_open || !process->infos[fd]->buffer->read_open) {
      release(&process->infos[fd]->buffer->lock);
      releasesleep(&process->infos[fd]->lock);
      return -1;
    }

   
    // There is now space to write what we want to write, so put it into the buffer
    for(int i = 0; i < left_to_write; i++) {

      // There is currently not enough room to write based on the amount
      // of data already in the buffer. We need to wait for there to be more space
      // since we don't allow partial writes. Additionally, if another writer is in the process of writing then
      // do that one
      while(process->infos[fd]->buffer->size == process->infos[fd]->buffer->buffer_size) {

        // Wake up any processes sleeping on the pipe buffer (AKA the read end)
        wakeup(process->infos[fd]->buffer);

        // Sleep until this is no longer the case
        sleep(process->infos[fd]->buffer, &process->infos[fd]->buffer->lock);

        // Now way to complete teh write, return -1
        if(!process->infos[fd]->buffer->write_open || !process->infos[fd]->buffer->read_open) { 
          release(&process->infos[fd]->buffer->lock);
          releasesleep(&process->infos[fd]->lock);
          return -1;
        }
      
      }

      process->infos[fd]->buffer->pipe_buf[process->infos[fd]->buffer->wr_offset] = buf[i];

      // If we go over the buffer edge then we wrap around
      if(process->infos[fd]->buffer->wr_offset+1 >= process->infos[fd]->buffer->buffer_size) {
        process->infos[fd]->buffer->wr_offset = 0;
      } else {
        process->infos[fd]->buffer->wr_offset++;
      }

      process->infos[fd]->buffer->size++;
    }

       

    // Wake up any processes sleeping on the pipe buffer (AKA the read end)
    wakeup(process->infos[fd]->buffer);

    // Release the lock
    release(&process->infos[fd]->buffer->lock);
    releasesleep(&process->infos[fd]->lock);


    // Return what we wrote
    return left_to_write;
}

int piperead(int fd, char* buf, int left_to_read) {

    struct proc* process = myproc();

    acquire(&process->infos[fd]->buffer->lock);


    // Read end is closed or write end is closed and there is no data
    if(!process->infos[fd]->buffer->read_open) {
      release(&process->infos[fd]->buffer->lock);
      return -1;
    }


    // There are currently no bytes to read
    // of data already in the buffer. We need to wait for there to be more space
    while(process->infos[fd]->buffer->size == 0) {

      
      // Check that the write end doesn't close while we are waiting for something to write
      if(!process->infos[fd]->buffer->write_open) {
        release(&process->infos[fd]->buffer->lock);
        return 0;
      }


      // Wake up anything else waiting to do actions on the buffer before
      // going to sleep
      wakeup(process->infos[fd]->buffer);
            
      // Sleep until this is no longer the case
      sleep(process->infos[fd]->buffer, &process->infos[fd]->buffer->lock);
    }

    // Check to see if the amount of bytes we want to read is greater than what is available
    if(process->infos[fd]->buffer->size < left_to_read) {
      left_to_read = process->infos[fd]->buffer->size;
    }

    // There is now data to read, so read it into the output buffer
    for(int i = 0; i < left_to_read; i++) {

      buf[i] = process->infos[fd]->buffer->pipe_buf[process->infos[fd]->buffer->rd_offset];

      // If we go over the buffer edge then we wrap around
      if(process->infos[fd]->buffer->rd_offset+1 >= process->infos[fd]->buffer->buffer_size) {
        process->infos[fd]->buffer->rd_offset = 0;
      } else {
         process->infos[fd]->buffer->rd_offset++;
      }

      process->infos[fd]->buffer->size--;
    }

    // Wake up any processes sleeping on the pipe buffer (AKA the write end)
    wakeup(process->infos[fd]->buffer);

    // Release the lock
    release(&process->infos[fd]->buffer->lock);
       
    // Return what we read
    return left_to_read;
  }
