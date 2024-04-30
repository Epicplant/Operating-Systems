# How Do Syscalls Return

<!-- Originally by: tenzinhl -->

This whitepaper goes more in-depth into the flow of execution as a syscall returns.

## What is the flow of execution as a `syscall` returns to userspace?

Here's a backtrace taken from `xk` when breaking on `sys_sleep`:
```
(gdb) where
#0  sys_sleep () at kernel/sysproc.c:68
#1  0xffffffff801071d6 in syscall () at kernel/syscall.c:170
#2  0xffffffff80109069 in trap (tf=0xffffffff8015af50) at kernel/trap.c:39
#3  0xffffffff80103960 in alltraps () at kernel/trapasm.S:20
#4  0x000000000000000d in ?? ()
#5  0x0000000000000000 in ?? ()
```

We can see that after `sys_sleep` completes, it'll return to `syscall`, which then returns to `trap`, which then returns to `alltraps`.

`alltraps` is written in assembly (see `trapasm.S`) and its execution flows into the `trapret` label. From here `trapret` does some stuff (to be explored later), then calls the special `iretq` function ("interrupt return"), which:
- pops some specially stored registers off the stack (which the processor put there when it first started processing the interrupt) 
    - including `%rip` (for next instruction to execute) and 
    - `%rsp` (for user stack)
- Switches the processor's privilege mode back to user mode.

In effect `iret` reverses a set of steps that the processor automatically does when it first starts the interrupt handling process. ([SOURCE](https://wiki.osdev.org/Interrupt_Service_Routines#x86-64))

## Okay, where in this process do we actually return a value?

`iretq` doesn't take any arguments (just like `ret`)... hrm... but then how do we normally return a value with `ret`? Well it's by following the `x86` calling conventions! `%rax` stores the first return value from a function!

So whatever is stored in `%rax` at the time we run the `iretq` instruction will be what the userspace code observes as the return value of our syscall.

## Okay, so where do we set `rax`?

We take a look at `trapret` again. `trapret` [pops a bunch of register values off of the stack to restore registers before it runs `iretq`](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/trapasm.S#L23-38), *including* `rax`!

Where are these values on the stack coming from? We actually saved them when we first called `alltraps` upon entering the kernel trap code! (i.e.: they correspond to the state of the processor in userspace right before this trap was initiated, i.e.: our trapframe!).[ And this set of register values on the kernel stack is actually tracked in our C-code as the `struct trap_frame*` passed to `trap`](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/9ca3893e6ebcb4a77e41ee1673dea6eb5a3bfbb1/kernel/trapasm.S#L19-20):
```
alltraps:
  push %r15
  [...]
  push %rax

  ; This line moves the stack pointer into %rdi,
  ; which by x86 calling convention is argument 1
  ; to the next function we call: trap!
  ; i.e.: %rsp is the `struct trap_frame*` pointer!
  mov %rsp, %rdi 
  call trap
```

(You'll note that the push order on `alltraps` matches the opposite order of the registers in `struct trap_frame`, it actually needs to so that putting the register variables in the right place for the C-code!)

"But wait, then `rax` will just store whatever it had in userspace before we `trap`'ed into the kernel, that can't be right..." and you'd be right!

We actually set `trap_frame->rax` in `syscall`, setting it to whatever the syscall we called returns ([LINK](https://gitlab.cs.washington.edu/xk-public/23au/-/blob/cb69599c63063e50f88695d25c3db004da7cee9d/kernel/syscall.c#L170)).

And so that's how your regular syscall returns a value to user space :). In summary:
- `sys_something` runs. Returns a value.
- `syscall` sets our `myproc()->tf->rax` to said return value.
- `trapret` runs and restores our processor state (including `%rax`) based on the trapframe
  - It then calls `iretq` resetting our stack pointer and instruction pointer (as well as privilege level) such that we're back in userspace.
