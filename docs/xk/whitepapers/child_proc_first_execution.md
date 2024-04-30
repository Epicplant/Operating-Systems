# What Do Newborn Processes Execute First

<!-- Originally by: tenzinhl -->

## Intro

So you call `allocproc` to allocate a new child process inside `fork`. You copy the virtual address space, you set up the trap frame, you're raring to go!

... but then what? How does that child actually execute?

From the user's perspective it's understandable how `fork` can seem a bit like magic. It's definitely one of the more unique programming APIs when you're first exposed to it. After calling it, suddenly we say there are two copies of that process running the same code with the same state (barring the `fork` return value). Huh? How?

## x86 is just a state machine

At the end of the day our OS and our processes are just code running on an x86-64 processor. A single core just chugs through instructions following a set of rules laid out in the ISA, updating its state accordingly. There's no magic `fork` instruction in the x86 ISA. There's just memory, registers, and code. The OS builds the abstraction of processes on top of the hardware (although the ISA is very much designed with multi-processing OS'es in mind, with special instructions and rules for interrupts, privilege levels, etc.).

## So what do new processes first execute in `xk`?

`allocproc` sets up the stuff that determines how a new child process runs when its first scheduled.

What a new process *first* executes when it's scheduled will be determined by its `context` (see ["Context Switching"](./context_switching.md)). When `allocproc` sets up a new PCB [it configures the `context` for the process such that the instruction pointer register (`%rip`) for the new proc's context to point to `forkret`](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/proc.c#L87).

But when `forkret` runs to completion it'll just return, but what will it's return address be? `allocproc` has us covered again. `allocproc` configures the kernel stack of our new process such that the return address on the stack after `forkret` completes points to `trapret`. This will then restore the processor state using the register values in `child->tf` before returning control to user land (where the next executed instruction will depend on what `%rip` is setup in `child->tf`).
