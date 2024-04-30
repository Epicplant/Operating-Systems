# Context Switching

<!-- Originally by: tenzinhl -->

This whitepaper goes more in-depth into how context switching is implemented in `xk`.

## What is context switching?

A "context" for our purposes is essentially a snapshot of the state of a process (all general purpose registers, and some special ones like `%rsp` (the stack pointer), `%rip` (the instruction pointer), `%cr3` (controls which page table is used)). (Note however, we store the vspace pagetable pointer outside of `struct context`, but I'd still consider it part of the context).

So "context switching" is, well, switching contexts. This is implemented in `swtch.S` ([LINK](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/swtch.S#L10)). It's as simple as:
- [Saving our current registers](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/swtch.S#L11-17) (see [comment above `swtch` implementation](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/swtch.S#L1-7) to see which registers are saved implicitly just by calling conventions)
- Saving the stack pointer into memory pointed to by `%rdi` (first argument). `%rsp` points at top of stack of where we just pushed our context, this is the `struct context` pointer for the context we just saved.
- Switching stack pointer to be where second argument points (`%rsi`)
- Restoring context from that other stack by just popping values off stack and executing `ret` instruction (which will use `ret` address saved on stack to return to where context originally was before calling `swtch`).

### swtch as having halves

A way to view `swtch` is that it has two halves. The first half occurs in one stack (the original context's stack), and the second in a different stack (the switched to process's stack). The halves are separated by when `%rsp` is switched.

When viewed as halves, `swtch` from the POV of a single process A is just a "single" function call, just that the second half doesn't occur immediately. The second half needs to be called from another process B (or from the scheduler rather, when process B or the scheduler `swtch`'es back to A).

It allows us to stitch together `swtch` calls all around! By doing this sort of stitching (the key being the stack pointer switch), `swtch` can just rely on `x86` calling conventions to handle setting up and changing the instruction pointer `%rip` (which is ultimately what's responsible for switching what code we run when context switching).

## What triggers context switches?

In `xk` there are two places in the code where we call `swtch` (can find them by doing an all-text search):
- Inside of the `scheduler()` function.
- Inside of the `sched()` function.
    - Which is called in `trap()`

`trap()` calls `yield()` which calls `sched()` on timer interrupts: [LINK](https://gitlab.cs.washington.edu/tenzinhl/cse451/-/blob/0676b081006081b40a760d606175f4bcba88c571/kernel/trap.c#L174-176).

### The Scheduler

`scheduler()` is the function that the scheduler logic runs. It's discussed more in [[The xk Scheduler]], but basically the scheduler's goal is to find the next runnable process (in a round robin loop) and schedule it immediately.

### The Timer

Timer have been present on motherboards since pretty much the beginning of PCs (and [nowadays are integrated into the chipset for x86 CPUs](https://stackoverflow.com/questions/12229644/time-sources-in-x86-processors)). Timers are just counters that count up at a consistent rate. When they overflow (or reach some threshold etc.) they can trigger interrupts. Being a separate device from the CPU, the timer will run and signal interrupts *regardless* of what the CPU is doing. *This* is what allows us to do [preemptive multitasking](https://en.wikipedia.org/wiki/Preemption_(computing)) (i.e.: we can do multi-process concurrency without having to trust that user programs are polite enough to yield the processor, [a strategy known as "cooperative multitasking"](https://en.wikipedia.org/wiki/Cooperative_multitasking#:~:text=Cooperative%20multitasking%2C%20also%20known%20as,running%20process%20to%20another%20process.) used in some systems (typically lower-level embedded systems)).

All interrupts in `xk` call into `alltraps`, which calls `trap` (so our timer will do this too). There's [some specific logic we run for timer interrupts](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/trap.c#L46-54), but the key for how this leads to a context switch to the scheduler is that for all non-fatal non-syscall traps, [we end up calling into the scheduler](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/trap.c#L106-110) (barring when process should die).

This is the bread-and-butter trigger of interrupts in `xk` that allows for multi-processing, without requiring that user programs manually call `syscalls` to yield the processor!

> Extension note for the curious: in `xk`, the timer is configured in `lapicinit`.
> 