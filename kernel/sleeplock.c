// Sleeping locks

#include <cdefs.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <sleeplock.h>
#include <spinlock.h>
#include <x86_64.h>

void initsleeplock(struct sleeplock *lk, char *name) {
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

// a sleeping lock relinquishes the processor if the lock is busy
// note mesa semantics: process can wakeup and find the lock still busy.
// NOTE: no spinlocks should be held while calling `acquiresleep`
// (since if `acquiresleep` tries to sleep while a spinlock is held,
// `sched`'s contract will be violated).
void acquiresleep(struct sleeplock *lk) {
  acquire(&lk->lk);

  // Make sure we never try to acquire sleeplock while we already hold it.
  if (lk->locked && lk->pid == myproc()->pid) {
    panic("Already holding sleeplock!");
  }

  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

// a sleeping lock wakes up a waiting process, if any, on lock release.
void releasesleep(struct sleeplock *lk) {
  acquire(&lk->lk);

  // Assert that the lock is actually locked.
  if (!lk->locked) {
    panic("releasesleep: sleeplock is not locked!");
  }

  // Assert that we are the one holding the lock.
  if (lk->pid != myproc()->pid) {
    panic("releasesleep: sleeplock is not held by current proc!");
  }

  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  release(&lk->lk);
}

int holdingsleep(struct sleeplock *lk) {
  int r;

  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}
