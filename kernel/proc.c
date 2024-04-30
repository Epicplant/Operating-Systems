#include <cdefs.h>
#include <defs.h>
#include <file.h>
#include <fs.h>
#include <memlayout.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <spinlock.h>
#include <trap.h>
#include <x86_64.h>
#include <fs.h>
#include <file.h>
#include <vspace.h>

// process table
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// Pointer to the init process set up by `userinit`.
static struct proc *initproc;

// Global PID counter used to assign unique increasing PIDs
// to newly created processes.
int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// to test crash safety in lab5,
// we trigger restarts in the middle of file operations
void reboot(void) {
  uint8_t good = 0x02;
  while (good & 0x02)
    good = inb(0x64);
  outb(0x64, 0xFE);
loop:
  asm volatile("hlt");
  goto loop;
}

void pinit(void) { initlock(&ptable.lock, "ptable"); }

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->killed = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0) {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trap_frame *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 8;
  *(uint64_t *)sp = (uint64_t)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->rip = (uint64_t)forkret;

  return p;
}

// Set up first user process.
void userinit(void) {
  struct proc *p;
  extern char _binary_out_initcode_start[], _binary_out_initcode_size[];

  p = allocproc();

  initproc = p;
  assertm(vspaceinit(&p->vspace) == 0, "error initializing process's virtual address descriptor");
  vspaceinitcode(&p->vspace, _binary_out_initcode_start, (int64_t)_binary_out_initcode_size);
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ss = (SEG_UDATA << 3) | DPL_USER;
  p->tf->rflags = FLAGS_IF;
  p->tf->rip = VRBOT(&p->vspace.regions[VR_CODE]);  // beginning of initcode.S
  p->tf->rsp = VRTOP(&p->vspace.regions[VR_USTACK]);

  safestrcpy(p->name, "initcode", sizeof(p->name));

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void) {

  // No need for argument checking since there are no parameters

  // Call allocproc to find a spot in the Global Process Table,
  // after which we return a reference to it. Additionally check that we don’t return 0 (AKA NULL) and immediately return -1 if it is.
  struct proc* child = allocproc();

  if(child == NULL) {
    return -1;
  }
  
  // Obtain current (the parent) process with myproc() call
  struct proc* parent = myproc();


  // In the new proc we acquired update it to be a copy of the current process
    
  // Copy over the vspace with vspaceinit (initializing a new vspace for the child) and vspacecopy
  if(vspaceinit(&child->vspace) == -1 ) {
    acquire(&ptable.lock);
    child->state = UNUSED;
    release(&ptable.lock);
    return -1;
  }
  
  
  if(vspacecowcopy(child, parent) == -1) {
    acquire(&ptable.lock);
    vspacefree(&child->vspace);
    kfree(child->kstack);
    child->state = UNUSED;
    release(&ptable.lock);
    return -1;
  }

  
  //PID handled by allocproc

  // Make the child's parent field point to parent
  child->parent = parent;

  // Copy over the trap frame (set %rax to 0 for child so the process knows it is a child process)
  struct trap_frame frame = *parent->tf;
  frame.rax = 0;
  *child->tf = frame;



  // Copy over file_info array in current proc (all of the pointers) so we have access to the same files
  for(int i = 0; i < NOFILE; i++) {
    if(parent->infos[i] != NULL) {
      acquiresleep(&parent->infos[i]->lock);
      child->infos[i] = parent->infos[i];
      child->infos[i]->reference++;
      releasesleep(&child->infos[i]->lock);
    }
  }


  // Call ptable lock so that when we are modifying the process in the ptable we don’t get
  // a write-write or a read-write race condition. Sequencer is constantly reading so we MUST lock otherwise we get a read-write
  acquire(&ptable.lock);

  // Set child's state to RUNNABLE
  child->state = RUNNABLE;

  // Don’t need to call sched() since we may want to continue running after forkingrun in a different context
  // Release the ptable lock since we are now past the point where we modify the global process tabl
  release(&ptable.lock);



  // Return child pid (process id in the global process table)
  return child->pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
  
  // Call myproc to get the current process
  struct proc* process = myproc();

  // Clean up as much memory as we can without actively deleting the ptable (AKA Clean up file_info 
  // array in the child since we don’t need these to be open for the exit call to finish running)
  for(int i = 0; i < NOFILE; i++) {
      fclose(i);
  }

  // Call ptable lock since we are about to modify the global process table
  // (which could be written to or read from by other processes concurrently)
  acquire(&ptable.lock);

  // Set the current processes state to “ZOMBIE” so that it can later be cleaned up by wait or initproc
  process->state = ZOMBIE;


  // Account for the fact that the parent might not call wait as it has exited already,
  // so go through every process, find its current children, and give it to initproc
  for(int i = 0; i < NPROC; i++) {
    if(ptable.proc[i].parent == process && ptable.proc[i].state != UNUSED) {
        ptable.proc[i].parent = initproc;
    }
  }

  wakeup1(process->parent->chan);

  
  sched();
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {

  // Call myproc to get the current process
  struct proc* process = myproc();

  acquire(&ptable.lock);

  bool childfound = false;
  // Check that the parent actually has a child that it can wait for
  for(int i = 0; i < NPROC; i++) {
    if(ptable.proc[i].parent == process && ptable.proc[i].state != UNUSED) {
        childfound = true;
        break;
    }
  }

  if(!childfound) {
    release(&ptable.lock);
    return -1;
  }



  // Infinitely loop through the process table until we
  // find a process whose parent is this process and whose
  // state is ZOMBIE or has the killed flag not be 0.
  while(1) {
    
    
    for(int i = 0; i < NPROC; i++) {
           
      // When we find a child process lock the process table and clean it up
      if(ptable.proc[i].parent == process && ptable.proc[i].state == ZOMBIE) {
        
        // Clean up any memory that wasn’t cleaned up by exit in the child process
        vspacefree(&ptable.proc[i].vspace);
        
        // Free the allocated kstack
        kfree(ptable.proc[i].kstack);
        
        ptable.proc[i].parent = NULL;

        // Set the process’s state to “UNUSED”  to signify that this spot can be used for another process
        ptable.proc[i].state = UNUSED;

        int returner = ptable.proc[i].pid;

        release(&ptable.lock);

        return returner;     
      }

    }
    
    // releases the lock and reacquires
    sleep(process, &ptable.lock);
  }

   // Should never get here. Eventually will catch a child having exited unless something
   // goes terrible wrong on the user's side.  
   return -1;
}

/*
 * arg0: integer value of amount of memory to be added to the heap. If arg0 < 0, treat it as 0.
 *
 * Adds arg0 to the current heap.
 * Returns the previous heap limit address, or -1 on error.
 *
 * Error condition:
 * Insufficient space to allocate the heap.  Note that if some space
 * exists but that space is insufficient to handle the complete request, 
 * -1 should still be returned, and nothing should be added to the heap.
 */
int sbrk(int amt) {

  // We don't allow negative amounts
  if(amt < 0) {
    amt = 0;
  }

  // Retrieve the virtual address of the top of the heap
  uint64_t ht = myproc()->vspace.regions[VR_HEAP].va_base
                + myproc()->vspace.regions[VR_HEAP].size;


  // Check that adding this amount to the heap won't overflow us into the bottom
  // of the stack. Return -1 if it does.
  uint64_t st = myproc()->vspace.regions[VR_USTACK].va_base
                - myproc()->vspace.regions[VR_USTACK].size;
  if(ht + amt >= st) {
    return -1;
  }

  // We only need to add new pages if where we start on a page combined with how much we want to
  // add goes over the size of a page. If it does we call vregionaddmap which handles adding the pages
  // we need.
  if(vregionaddmap(&myproc()->vspace.regions[VR_HEAP], ht, amt, VPI_PRESENT, VPI_WRITABLE) < 0)
    return -1; 

  // Increment the size of the heap
  myproc()->vspace.regions[VR_HEAP].size += amt;
  vspaceupdate(&myproc()->vspace);

  return ht;
}


// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
  struct proc *p;

  for (;;) {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      mycpu()->proc = p;
      vspaceinstall(p);
      p->state = RUNNING;
      swtch(&mycpu()->scheduler, p->context);
      vspaceinstallkern();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      mycpu()->proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void) {
  int intena;

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1) {
    // Only the ptable lock should be held, thus ncli should be exactly 1, and
    // any other sources of `pushcli` should have gracefully called `popcli`. We
    // enforce that ncli == 1 because otherwise interrupts would be permanently
    // disabled while scheduling the next thread.
    cprintf("pid : %d\n", myproc()->pid);
    cprintf("ncli : %d\n", mycpu()->ncli);
    cprintf("intena : %d\n", mycpu()->intena);

    // If you are getting this output, it most likely means that you're
    // holding a spinlock other than the ptable.lock while trying to
    // enter the scheduler.
    panic("sched locks");
  }
  if (myproc()->state == RUNNING)
    panic("sched running");
  if (readeflags() & FLAGS_IF)
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&myproc()->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
  acquire(&ptable.lock); // DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk) {
  if (myproc() == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),waku
  // so it's okay to release lk.
  if (lk != &ptable.lock) { // DOC: sleeplock0
    acquire(&ptable.lock);  // DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  myproc()->chan = chan;
  myproc()->state = SLEEPING;
  sched();

  // Tidy up.
  myproc()->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock) { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan) {
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid) {
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void) {
  static char *states[] = {[UNUSED] = "unused",   [EMBRYO] = "embryo",
                           [SLEEPING] = "sleep ", [RUNNABLE] = "runble",
                           [RUNNING] = "run   ",  [ZOMBIE] = "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint64_t pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state != 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING) {
      getcallerpcs((uint64_t *)p->context->rbp, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

struct proc *findproc(int pid) {
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid)
      return p;
  }
  return 0;
}
