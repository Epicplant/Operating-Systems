# Lab 3: Address Space Management
## Design Doc Due: 02/08/2024 at 11:59pm. No late days.
## Lab Due: 02/16/2024 at 11:59pm.

Everything is turned in on **Gradescope**. One submission per assignment per group, and make sure to add your group members on Gradescope!

- [Configuration](#configuration)
- [Introduction](#introduction)
- [Part 1: Create a user-level heap](#part-1-create-a-user-level-heap)
- [Part 2: Starting shell](#part-2-starting-shell)
- [Part 3: Grow user stack on-demand](#part-3-grow-user-stack-on-demand)
- [Part 4: Copy-on-write fork](#part-4-copy-on-write-fork)
  - [Keyword Glossary](#keyword-glossary)
  - [Implementation Tips](#implementation-tips)
- [Wrapping Up](#wrapping-up)
- [Testing](#testing)
  - [lab3test expected output](#lab3test-expected-output)
- [Hand-in](#hand-in)


## Configuration

To merge lab3 handout and tests, run:
```bash
# Checkout main (or whatever branch you want to merge changes into)
git checkout main
git pull upstream main
```

## Introduction

In this lab, we are going to cover address space management. With it, you will
be able to run `xk`'s shell. We also ask you to implement some common techniques to optimize memory usage.

### Setting up the init process

At this point, you may have noticed that we've been running the tests (`user/lab1test.c` and `user/lab2test.c`) by compiling them into the `initcode` binary, which is then compiled and linked directly into the kernel binary.
Currently, `kernel/initcode.S` contains the entry point of the `initcode` binary (the `start` label). `initcode` then calls the `main` function of `user/lab2test.c`. 


We now change `kernel/initcode.S` so that it calls `exec` with a predefined filepath. 
This allows the user to set what the initial user program is without recompiling the kernel, by just changing what program lives under that predefined filename on disk.

To do this, look at `kernel/Makefrag`, you will find this section:
```
$(O)/initcode : kernel/initcode.S user/lab2test.c $(ULIB)
	$(CC) -nostdinc -I inc -c kernel/initcode.S -o $(O)/initcode.o
	$(CC) -ffreestanding -MD -MP -mno-sse -I inc -c user/lab2test.c -o $(O)/lab2test.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x10000 -o $(O)/initcode.out $(O)/initcode.o $(O)/lab2test.o $(ULIB)
	$(OBJCOPY) -S -O binary $(O)/initcode.out $(O)/initcode
	$(OBJDUMP) -S $(O)/initcode.out > $(O)/initcode.asm
```

Remove the dependency of `user/lab2test.c` in the initcode make rule by replacing the snippet above to the following:
```
$(O)/initcode : kernel/initcode.S
	$(CC) -nostdinc -I inc -c kernel/initcode.S -o $(O)/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x10000 -o $(O)/initcode.out $(O)/initcode.o
	$(OBJCOPY) -S -O binary $(O)/initcode.out $(O)/initcode
	$(OBJDUMP) -S $(O)/initcode.o > $(O)/initcode.asm
```

Also change the content of `kernel/initcode.S` to
```
#include <syscall.h>
#include <trap.h>

.globl start
start:
  mov $init, %rdi
  mov $argv, %rsi
  mov $SYS_exec, %rax
  int $TRAP_SYSCALL

exit:
  mov $SYS_exit, %rax
  int $TRAP_SYSCALL
  jmp exit

init:
  .string "/lab3init\0"

.p2align 2
argv:
  .quad init
  .quad 0
```

> Here `"/lab3init"` is the predefined filepath we expect the initial user program to live under.

Run `make clean` to make sure these Makefile changes are reflected next time you build.

After you change the parts above, xk will still execute `kernel/initcode.S` as it prepares the initial user process. However, unlike before, 
`kernel/initcode.S` will now load `user/lab3init.c` on the disk, which will open the console and exec `user/lab3test.c`. 
Later on we will change this to load into a shell! (Once your kernel supports the `sbrk` syscall).

## Part 1: Create a user-level heap

A process that needs more memory at runtime can call `sbrk` (set program break) to grow its
heap size. The common use case is the situation where a user library routine,
`malloc` in C or `new` in C++, calls `sbrk` whenever the application
asks to allocate a data region that cannot fit on the current heap
(e.g., if the heap is completely allocated due to prior calls to `malloc`). For example, if a user application wants to increase the
heap size by `n` bytes, it calls `sbrk(n)`.

In UNIX, the user application can also decrease the size of the heap by passing
negative values to `sbrk`, but you do not need to support this. Generally, the
user library will ask `sbrk` to provide more space than immediately needed to make fewer system calls.

When a user application is loaded via `exec`,
the user program is initialized to have a zero-size heap
(i.e., `vregion[VR_HEAP].size = 0`), and so the first call to `malloc`
always calls `sbrk`.

When it comes to a user process's heap during the process's lifetime, `xk` is responsible for:
1. Tracking how much memory has been allocated to the heap.
2. Allocating physical memory for the heap.
3. Updating virtual memory mappings for the heap.

For (1) recall that `xk` allocates and frees memory at page granularity
(i.e., 4096 bytes) but `sbrk` needs to support allocating/deallocating memory at
byte granularity. The OS does this to be portable, since an application cannot depend
on the machine adopting a specific page size.

(2) can be achieved through `vregionaddmap` (which `kalloc`'s pages for all memory mappings you add with it).

(3) is also partially achieved through `vregionaddmap`, however `vregionaddmap` only updates `xk`'s platform-agnostic virtual address space datastructure (`struct vspace`). To actually have those changes reflected to the CPU, you're implementation will need to use `vspaceupdate` to regenerate the hardware-specific page table.

In user space, we have provided an implementation of `malloc` and `free` (in `user/lib/umalloc.c`) that is going to use `sbrk`. After the implementation of `sbrk` is
done, user-level applications should be able to call `malloc` and `free`.

#### Exercise

> Implement `sys_sbrk()` in `kernel/sysproc.c`.

```c
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
int
sys_sbrk(void);
```

#### Question #1
Why might an application prefer using `malloc` and `free` instead of using `sbrk` directly?

#### Question #2
What is the relationship between `malloc`/`free` and `sbrk`?

<!-- ### Optional Exercise
Implement `sbrk` with decrement. Above, we said if `n < 0`, we treat `n` as 0. However,
`sbrk(n)` is used to deallocate memory in the kernel as well. The kernel would deallocate
`abs(n)` bytes of memory, and returns the previous heap limit. If the allocated memory is
less than `abs(n)` bytes (not enough memory to deallocate), it would behave the same as `sbrk(0)`.

Hint: Implement a `vregiondelmap` in `kernel/vspace.c`. Be aware of signed/unsigned integer arithmetic. -->

## Part 2: Starting shell

Now that you've implemented `sbrk` we can run the shell! A shell is a typical user interface for operating systems, and a basic implementation is provided for you in `user/sh.c`. We're going to run the shell as the kernel's initial process, and going forward you'll be able to use the shell to load all tests and user programs from disk.

In order to run the shell, change this line in `kernel/initcode.S`:
```
init:
  .string "/lab3init\0"
```
to
```
init:
  .string "/init\0"
```
> Now `"/init"` is the predefined filepath the kernel looks to execute in the initial user process.

After you change the parts above, xk will start with `kernel/initcode.S`, but
will `exec` the binary compiled from `user/init.c` instead of `user/lab3init.c`. 
`user/init.c` will fork into two processes. One will `exec` `user/sh.c`, the other
will wait for zombie processes to reap. After these changes, when you boot xk,
you should see the following:

```
Booting from Hard Disk..xk...
CPU: QEMU Virtual CPU version 2.5+
  fpu de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2
  sse3 cx16 hypervisor
  syscall nx lm
  lahf_lm svm
E820: physical memory map [mem 0x9000-0x908f]
  [mem 0x0-0x9fbff] available
  [mem 0x9fc00-0x9ffff] reserved
  [mem 0xf0000-0xfffff] reserved
  [mem 0x100000-0xfdffff] available
  [mem 0xfe0000-0xffffff] reserved
  [mem 0xfffc0000-0xffffffff] reserved

cpu0: starting xk

free pages: 3730
cpu0: starting
sb: size 50000 nblocks 49985 bmap start 2 inodestart 15
init: starting a new shell
$ 
```

At this point, you can execute your tests in the shell. 
In GDB, the `initcode` command will load the symbol table for `init`, not the currently running test code. 
To step through the test code, use, for example, `lab3test` to load symbols for `lab3test.c`. (See `.gdbinit.tmpl` for the other predefined `GDB` macros).

#### Exercise

> Allow xk to boot into the shell. Try a set of commands like `cat`, `echo`, `grep`,
> `ls`, `wc` along with `|` (i.e. pipe) in the shell to see if it works properly.

We will be testing the following commands for this lab:
```
echo "hello world" | wc
cat small.txt | wc
ls
```

#### Question #3:

How many child processes are created by the shell program in order to run the command `ls | wc`? (This mirrors real OS'es).

> Hint: the shell will go into the `case PIPE` case in `user/sh.c:runcmd` when it receives a cmd with the pipe operator `|`.

> Fun fact: the  `|` operator behaves this way in most UNIX shells, and is why the infamous forkbomb command `:(){ :|:& };:` *forks*. [More on the forkbomb LINK](https://en.wikipedia.org/wiki/Fork_bomb).

#### Question #4:

The shell will call `pipe()` when the command `ls | wc` is run. What does the shell do with the read end? What does the shell do with the write end? (~1-2 sentences).

> Hint: once again look at `case PIPE` in `user/sh.c:runcmd`.

## Part 3: Grow user stack on-demand

In the rest of lab3, we study how to reduce xk's memory consumption. The first
technique is to grow the user stack on-demand. In your implementation of `exec`,
the user stack size is fixed and is allocated before the user application starts.
However, we can change that to allocate only the memory that is needed
at run-time. Whenever a user application issues an instruction that
reads or writes to the user stack (e.g., creating a stack frame, accessing local
variables), we grow the stack as needed.

When the user process starts, you should set up the user stack with
an initial page to store application arguments. Currently, the stack is allocated
3 pages. You should change it so that initially, only 1 page is allocated.

#### Exercise

> Modify `vspaceinitcode` and `vspaceinitstack` so they
> only allocate a single stack page when a process is created.

To implement on-demand stack growth, you will need to understand how to
handle page faults.
A page fault is a hardware exception that occurs when a program accesses
a virtual memory page without a valid page table entry, or with a valid
entry, but where the program does not have permission to perform the
operation.

When the hardware exception is raised, control will trap into the kernel; the exception
handler should add memory to the stack region and resume execution in the user program.

The code for handling this can be seen in `trap.c`.
```c
  default:
    addr = rcr2();

    if (tf->trapno == TRAP_PF) {
      num_page_faults += 1;
```
In the case of a page fault, `rcr2()` returns the address that was attempted to be accessed. <br />
More information on the page fault can be obtained by examining `tf->err`.
The last 3 bits indicate key information. <br />
b<sub>31</sub>...b<sub>2</sub>b<sub>1</sub>b<sub>0</sub><br />
b<sub>2</sub> is set if the fault occurred in usermode.<br />
b<sub>1</sub> is set if the fault occurred on a write.<br />
b<sub>0</sub> is set if it was a page protection issue. If not set, then the page was not present (i.e.: not mapped).<br />
More info can be found [HERE](https://wiki.osdev.org/Exceptions#Page_Fault).

**In this lab, you should design your stack grower to never exceed 10 stack pages.**

#### Question #5:

When a syscall completes, user-level execution resumes with the instruction
immediately after the syscall. When a page fault exception completes, where does
user-level execution resume? (~1 sentence).

#### Question #6:

How should your `xk` implementation decide whether an unmapped reference is a
normal stack operation versus a stray pointer dereference that should cause the
application to halt?

> Hint: keep in mind that your stack grower in `xk` should allow the user stack to grow to 10 pages, and no more than 10 pages.

#### Exercise

> Implement growing the user stack on-demand.
> You will need to add page fault handling logic in `trap` (`kernel/trap.c`).
> Note that our test code uses the system call `sysinfo` to figure out how much memory is used.

#### Question #7:

Is it possible to reduce the user stack size at
run-time (i.e., to deallocate the user stack when a procedure with a
large number of local variables goes out of scope)? If not, explain why.
If so, sketch how that might work. (~1-3 sentences).

### Warning

At this point we will be editing fork. It is possible to get in a state where
no process will be able to start. We strongly suggest you checkpoint your code
in some way (using a git tag, or copying the code into a directory you won't touch).

## Part 4: Copy-on-write fork

The next optimization improves the performance of fork by
using a copy-on-write mechanism. Currently, `fork` duplicates every page
of user memory in the parent process.  Depending on the size
of the parent process, this can consume a lot of memory
and can potentially take a long time.  All this work is thrown away
if the child process immediately calls `exec` (which is a common pattern).

Here, we reduce the cost of `fork` by allowing multiple processes
to share the same physical memory, while at a logical level
still behaving as if the memory was copied.  As long as neither process
modifies the memory, it can stay shared; if either process changes
a page, a copy of that page will be made at that point
(that is, copy-on-write).

When `fork` returns, the child process is given a page table that
points to the same memory pages as the parent.  No additional memory
is allocated for the new process, other than to hold the new page table.
However, the page tables of both processes will need to
be changed to be read-only.  That way, if either process tries
to alter the content of their memory, a trap to the kernel will occur,
and the kernel can make a copy of the memory page at that point,
before resuming the user code. As a start, it will be helpful to understand
how `vspacecopy` works, and then implement your own `vspacecowcopy`.

Once you implement copy-on-write, multiple processes can map to the
same set of frames (physical pages) in physical memory. You will need to track how many virtual pages 
are mapped onto each frame in order to know when a frame can be freed. 
You will also need to make sure that your updates to the reference count are protected.
You can allocate a frame via `kalloc`, and each frame has an associated `core_map_entry` that
you can use to store information. You can modify `kalloc` and `kfree` to work with your newly added field.

For every page in a process's page table (vspace), there is a structure that
represents that entry (`struct vpage_info`). Feel free to add fields,
potentially to indicate this virtual page is a copy-on-write page. On a page
fault, you can use the vspace functions (`va2vregion` and `va2vpage_info`) to
get the `vpage_info` for the faulting address and understand what caused the fault.

On a page fault your implementation will need to determine if the virtual address is a copy-on-write address.
If it is, you can allocate a page, copy the data from the copy-on-write page,
and let the faulting process start writing to that freshly-allocated page.
Note that whenever you are changing the mapping or changing the permission of 
a virtual page, you need to flush the TLB to make sure you don't use the cached stale mapping (see [Question 9](#question-9)).

A tricky part of the assignment is that, of course, a child process
with a set of copy-on-write pages can fork another child.
Thus, a physical frame can be referenced by multiple processes.
There are various possible ways of keeping track of which pages
are copy-on-write; we do not specify how. Instead, we will only test for
functional correctness -- that the number of allocated pages is
small and that the child processes execute as if they received a
complete copy of the parent's memory.

#### Question #8:

The TLB caches the page table entries of recently referenced
pages. When you modify the page table entry to allow write access,
which function in `vspace.c` should you use to ensure that the TLB does not have a stale version of the page table entry you modified?

> Hint: in x86-64 modifying the `CR3` register changes what page table the CPU uses and flushes the TLB. Look for a function that modifies the `CR3` register.

#### Exercise

> Implement copy-on-write fork. You will need to modify `fork` (`kernel/proc.c`), 
> relevant functions in `kernel/vspace.c`, and add in page fault logic in `trap` (`kernel/trap.c`).

### Keyword Glossary

You will see a lot of addresses and bookkeeping structures in this lab:
- **user virtual address**: this is a virtual address within a process's virtual address space, it will be between `[0, SZ_2G)`
- **`struct vpage_info`**: each page in a process's virtual address space has a corresponding `struct vpage_info`, you can use it to track page specific info. 
To get the `vpage_info` for a virtual address, you need to first find the `vregion` of the address (`va2vregion`), and then you can use `va2vpage_info`.
- **kernel virtual address**: this is a virtual address within the kernel's virtual address space, it will be above `KERNBASE`. The kernel code, data, and 
  heap all use kernel virtual addresses (print out the address of any kernel code/data to see it for yourself!). When you call `kalloc`, a kernel 
  virtual address is returned upon success, the returned address is guaranteed to be backed by a physical frame. You can use this to allocate dynamic data in the kernel (kstack, pipe),
  or you can hand this to a user process by mapping a process's page to this frame. For addresses returned by `kalloc`, you can call `V2P` to get the physical address backing this page.
- **`struct core_map_entry`**: this is a bookkeeping structure for each physical frame. To retrieve this, you can call `pa2page` with the physical address.
- **`P2V`**: get a kernel virtual address from a physical address
- **`V2P`**: get a physical address from a kernel virtual address
- **`ppn`**: "Physical Page Number". Every physical frame of memory has a unique id counting from 0 to (the number of frames - 1). That unique id is the page's PPN.

### Implementation Tips

Some tips for manipulating said datastructures:
- **To get the physical address of a `vpage_info`**, leftshift `vpage_info->ppn` by `PT_SHIFT` (ex. `vpage_info->ppn << PT_SHIFT`)
- **To get `ppn` from a core_map_entry**, you can use `PGNUM(page2pa(page))` or `page - core_map`.
  - The virtual address the core_map_entry stores does not have a 1-to-1 relationship with PPN's, and thus should not be used to get the PPN.
- **Some functions require physical address, some virtual**. Pay attention to this detail.
- **Be careful when checking for reference counts in `kfree`**. On boot, the system calls `kfree` on each page BEFORE ever calling `kalloc`. This means it's possible for reference counts to be 0 in kfree. 

## Wrapping Up

#### Question #9

For each member of the project team, how many hours did you
spend on this lab?

#### Question #10

What did you like or dislike about this lab? Is there anything you wish you knew earlier?

## Testing
After you implement the system calls described above. The kernel should be able
to print `lab3 tests passed!` when you run `lab3test` from the shell. You should also use `sysinfo` in the shell to
detect potential memory leaks when running `lab3test`. If your implementation
is correct, `pages_in_use` should be kept the same before and after running `lab3test`.

Running the tests from the _previous_ labs is a good way to boost your confidence
in your solution for _this_ lab.

> While not strictly necessary, to be more "accurate" you can comment out
> the lines that call `open` and `dup` on the console in `lab1test.c` and `lab2test.c`
> since `init` now sets up `stdin/stdout/stderr` for all child processes that fork off of it.
> (Including the tests).

### lab3test expected output
lab3test output should be close to the following: 
```
init: starting a new shell
$ lab3test
(lab3) > all
bad_mem_access -> 
pids 4-43 (6-45 if ran after sh) should be killed with trap 14 err 5
pid 4 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80000000--kill proc
pid 5 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8000c350--kill proc
pid 6 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800186a0--kill proc
pid 7 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800249f0--kill proc
pid 8 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80030d40--kill proc
pid 9 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8003d090--kill proc
pid 10 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800493e0--kill proc
pid 11 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80055730--kill proc
pid 12 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80061a80--kill proc
pid 13 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8006ddd0--kill proc
pid 14 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8007a120--kill proc
pid 15 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80086470--kill proc
pid 16 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800927c0--kill proc
pid 17 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8009eb10--kill proc
pid 18 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800aae60--kill proc
pid 19 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800b71b0--kill proc
pid 20 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800c3500--kill proc
pid 21 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800cf850--kill proc
pid 22 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800dbba0--kill proc
pid 23 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800e7ef0--kill proc
pid 24 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x800f4240--kill proc
pid 25 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80100590--kill proc
pid 26 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8010c8e0--kill proc
pid 27 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80118c30--kill proc
pid 28 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80124f80--kill proc
pid 29 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x801312d0--kill proc
pid 30 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8013d620--kill proc
pid 31 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80149970--kill proc
pid 32 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80155cc0--kill proc
pid 33 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80162010--kill proc
pid 34 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8016e360--kill proc
pid 35 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8017a6b0--kill proc
pid 36 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80186a00--kill proc
pid 37 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x80192d50--kill proc
pid 38 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x8019f0a0--kill proc
pid 39 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x801ab3f0--kill proc
pid 40 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x801b7740--kill proc
pid 41 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x801c3a90--kill proc
pid 42 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x801cfde0--kill proc
pid 43 lab3test: trap 14 err 5 on cpu 0 rip 0x102c5 addr 0x801dc130--kill proc
pid 44 lab3test: trap 14 err 4 on cpu 0 rip 0x103b6 addr 0x21000--kill proc
passed 
malloc_test -> passed 
sbrk_small -> passed 
sbrk_large -> passed 
stack_growth_basic -> 
pages_in_use before stack allocation = 389
pages_in_use after stack allocation = 397
passed 
stack_growth_bad_access -> 
next 2 processes should be killed with trap 14 err 6
pid 48 lab3test: trap 14 err 6 on cpu 0 rip 0x10f4b addr 0x7fff5000--kill proc
pid 49 lab3test: trap 14 err 6 on cpu 0 rip 0x11062 addr 0x80000000--kill proc
passed 
cow_fork -> 
cow_fork: pages_in_use before copy-on-write fork = 598
cow_fork: pages_in_use after copy-on-write fork = 638
cow_fork: pages_in_use after read = 638
cow_fork: pages_in_use after write = 838
passed 
passed lab3 tests
(lab3) > 
```


## Hand-in

Please submit your answers to the questions listed above to Gradescope.

To submit the code, zip your repo and **submit to Gradescope**.

**If you have a partner, make sure to add them to your submissions on Gradescope!**

> You may find it useful to tag your submission for future reference, but you will only be graded on what you submit to Gradescope. To tag the latest commit, use `git tag lab3_final && git push origin main --tags`. We'd also recommend keeping your answers to the lab questions stored in the repo (perhaps under the `labs/` folder).
