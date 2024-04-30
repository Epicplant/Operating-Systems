# The `xk` Scheduler

<!-- Originally by: tenzinhl -->

A quick overview of how the `xk` scheduler do.

## Intro

In `xk` there is one scheduler per CPU (and for our setup there's only one CPU, so there's one scheduler overall). The scheduler's goal is to hand control to processes that are ready to run in some "fair" way.

It's logic is contained in [`scheduler()` in `proc.c`](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/proc.c#L150).

## The scheduler as a "process"

If you really squint your eyes, the `xk` scheduler can be thought of as a sort of pseudo-process (although in many ways it is not, see addendum for details). It's logic runs whenever `sched()` is called to context switch to the scheduler.

The scheduler *is* in some sense just the `scheduler` function. Per CPU, it's entire execution lives entirely within the infinite for loop in `scheduler()`.

From the perspective of one CPU's scheduler, it hands control to a single process at a time using `swtch()` ([LINK](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/proc.c#L166-169)), then later on when said process hands control back by calling `sched()` the scheduler just continues running from where it was after we context switch back to scheduler. (Note that the process doesn't necessarily context switch back to scheduler "willingly", timer interrupts will force context switches back to the scheduler).

## It's a Round-Robin Scheduler

The scheduler is a simple round-robin scheduler. This just means it iterates through the process table from start to finish, running each process if its ready. When it reaches the end of the list it just starts from the beginning again. To guard against race conditions it acquires the ptable lock while doing this traversal (and is also why you need to hold the ptable lock when calling `sched`, should that be something you do).

NOTE: the scheduler only runs processes which have their state set to `RUNNABLE`. This is a large part of the value of the `proc->state` field. 
