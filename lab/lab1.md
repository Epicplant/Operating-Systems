# Lab 1: Interrupts and System Calls

## Lab Due: 01/15/2024 at 11:59pm.

## Introduction
Every lab in this course is built on top of a skeleton operating system (xk) designed for 
educating students about the principles and real-world practices of operating systems.
xk is a 64 bit port of [xv6](https://github.com/mit-pdos/xv6-public) by MIT with some minor changes.
It's a simple bootable operating system with a set of basic system calls.
As of now, it is capable of running a single user program that prints out "hello world" to the console.
Throughout the quarter, you will add new functionalities to xk to transform it into a more practical and usable system.

In this lab, you will learn to run and debug xk, implement new system calls,
and answer questions that will enhance your understanding of xk.

> Before continuing, it may be helpful to checkout [How to Read Specs](./how_to_read_specs.md) to understand what's expected from you as you read through this spec.

## Setting up your lab repo
The files you will need for this and subsequent lab assignments in
this course are distributed using the Git version control system.
To learn more about Git, take a look at the
[Git user's manual](https://git-scm.com/docs/user-manual).
The course Git repository is on the [CSE GitLab](https://gitlab.cs.washington.edu/xk-public/24wi).
Follow the instructions there to set up your ssh keys if you don't already have one.

We strongly recommend that you develop on the [CSE attu](https://www.cs.washington.edu/lab/linux) machines. 
You can log into it via (`ssh uwid@attu.cs.washington.edu`).
You can develop on `attu` using a text editor of your choice, and [`vscode.md`](../docs/development/vscode.md)
provides instructions for setting up VSCode for remote development on `attu`.
If you have trouble accessing attu, please contact the course staff as soon as possible.

Once you are logged in to `attu`, you can clone the course repository by running the commands below.

```
$ git clone git@gitlab.cs.washington.edu:xk-public/24wi.git xk
Cloning into xk...
$ cd xk
```

Next, you need to create your own repository on gitlab instead of working directly on our repo.
**Only one person from each group** needs to do the following:
1. create a new project on your gitlab with the blank template (**do not** initialize your repo with a README)
2. set the visibility of your newly created repo to **private**, your code should not be visible to other students
3. add your team member(s) as Maintainer to your repo
4. add all course staffs as Developer to your repo, make sure to add via emails (can be found [here](https://courses.cs.washington.edu/courses/cse451/24wi/index.html#staff))
5. populate your repo with the xk codebase by running the following commands.
  ```
  $ git remote rename origin upstream
  $ git remote add origin git@gitlab.cs.washington.edu:<repo_owner's_uwid>/<repo_name>.git
  $ git push -u origin --all
  ```

Once the group repo is set up, other team members should directly pull the newly created repo with the following commands:
```
$ git clone git@gitlab.cs.washington.edu:<repo_owner's_uwid>/<repo_name>.git
$ cd <repo_name>
$ git remote add upstream git@gitlab.cs.washington.edu:xk-public/24wi.git
```

## Running xk
Now we are ready to compile and run xk!
xk is an operating system, which means it should run directly on top of real machines.
But instead of developing and running it on a real, physical machine, 
we use a program that faithfully emulates a complete machine: 
the code you write for the emulator will boot on a real machine too. 

Using an emulator simplifies debugging; you can, for example, set break points inside the emulated x86_64, 
which is difficult to do with the silicon version of an x86_64.
So, for our labs we will use the [QEMU Emulator](https://wiki.qemu.org/Main_Page) to run our operating system.
We provide a version of QEMU that emulates x86_64 machines on attu. 
To access it, add the following line to your shell startup file (`~/.bashrc` for bash).
```
export PATH=/cse/courses/cse451/24wi/bin/x86_64-softmmu:$PATH
```
You can then run 
```
source ~/.bashrc
```
to reload your `bashrc` after the change.

Now you are ready to compile and run xk! 
First, type `make` in your project directory to build xk.
Next, type `make qemu` to run xk with QEMU.

You should observe the following output:
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

free pages: 3751
cpu0: starting
sb: size 50000 nblocks 49985 bmap start 2 inodestart 15
hello world
```

To exit the QEMU virtual instance, do `Ctrl-a, x` (press ctrl and a, release, and then press x). 

How does it work? On `make`, we first compile the xk kernel and user applications, 
then write the bootloader and the kernel binary into the kernel image file `out/xk.img`, 
and write the user programs and file system metatdata into the file system image file `out/fs.img`.
On `make qemu`, we specify a number of QEMU options (how much memory does this emulated machine have, how many cpu, 
direct serial port output to the terminal, etc.), 
and supply these files to QEMU as the content of the emulated machine's virtual hard disk.

And now you are ready to start lab1!

## Organization of source code

```
xk
├── inc           // all the header files (i.e., .h); includes definition of all the data structures and the system call interfaces
├── kernel        // the kernel source code
│   └── Makefrag  // compilation scripts for kernel (xk.img), QEMU scripts
├── user          // all the source code for user applications
│   └── Makefrag  // compilation scripts for user applications
├── lab           // specifications for labs
├── Makefile      // starting point of compilation
└── sign.pl       // make sure the boot block is below a block size on the disk
```

After compilation, a new folder `out` will appear, which contains the kernel image and all the intermediate compilation outputs (.d, .o, .asm).

## Part 1: How to debug xk

The purpose of the first exercise is to introduce you to the kernel bootstrap process and to get you started with QEMU/GDB debugging.
You will not have to write any code for this part of the lab, but you will use GDB and answer questions.
Write down your answers in a .txt file, each group only needs to submit one submission on Gradescope.

<!-- 
### Getting started with x86_64 assembly

The definitive reference for x86_64 assembly language programming is Intel’s
instruction set architecture reference is
[Intel 64 and IA-32 Architectures Software Developer’s Manuals](https://software.intel.com/en-us/articles/intel-sdm).
It covers all the features of the most recent processors that we won’t need in class, but
you may be interested in learning about. An equivalent (and often friendlier) set of
manuals is [AMD64 Architecture Programmer’s Manual](http://developer.amd.com/resources/developer-guides-manuals/).
Save the Intel/AMD architecture manuals for later or use them for reference when you want
to look up the definitive explanation of a particular processor feature or instruction.

You don't have to read them now, but you'll almost certainly want
to refer to some of this material when reading and writing x86_64 assembly. -->

### GDB

While QEMU's built-in monitor provides only limited debugging support, QEMU can act as a remote debugging target for GDB, 
which we'll use in this lab to step through the early boot process.
Before you can start using GDB, you have to add our given .gdbinit to be allowed on your machine. 
Create a file `~/.gdbinit` in your home directory and add the following line:
```
add-auto-load-safe-path /absolute/path/to/xk/.gdbinit
```
(where `/absolute/path/to/xk/` is the absolute path to your xk directory, use `pwd -P` to check your absolute path)

To attach GDB to xk, you need to open two separate terminals, both in the xk root directory.
In one terminal, type `make qemu-gdb`. This starts a QEMU instance and wait for GDB to attach. 
In another terminal, type `make gdb`. Now the GDB process is attached to QEMU. 

Note that when you are using `attu` there are different attu servers that you might be connected to.
Make sure both of your terminals are connected to the same physical attu server (e.g., explicitly using `attu2.cs.washington.edu`).

In xk, when bootloader loads the kernel from disk to memory, the CPU operates in 32-bit mode. 
The starting point of the 32-bit kernel is in `kernel/entry.S`. 
`kernel/entry.S` setups 64-bit virtual memory and enables 64-bit mode.
You don't need to understand `kernel/entry.S` in detail, but take a look at 
[this section](https://gitlab.cs.washington.edu/xk-public/24wi/-/blob/main/kernel/entry.S#L126-139) after the switch to 64-bit.
You will see a call to `main` in `kernel/main.c` which is the starting point of the OS.

#### Question #1:
In the GDB script (`.gdbinit.tmpl`) that is provided, we already set a breakpoint at the entrance of xk (`main` in `kernel/main.c`). 
xk will go through the booting and stop at `main`. Which line of code in `main` (a) prints the cpu information and (b) prints the physical memory table? (Hint: use the `n` command)

Please write down both the line number and the code in the line.

#### Question #2:
We can examine memory using GDB’s `x` command. The GDB manual has full details, 
but for now, it is enough to know that the command `x/Nx ADDR` prints `N` words of memory at `ADDR`. 
(Note that both ‘x’s in the command are lowercase)

To examine instructions in memory (besides the immediate next one to be executed, which GDB prints automatically), use the `x/i` command. 
This command has the syntax `x/Ni ADDR`, where `N` is the number of consecutive
instructions to disassemble and `ADDR` is the memory address at which to start
disassembling.

What's the memory address of `main` function for the kernel? (Hint: the `print`
command can be used to print many things, including the address of functions).

Does GDB work with real physical addresses or virtual addresses?
Please also explain how you found the answer to the last part of this question.

## Part 2: Memory Management
Please take a look at the [memory management in xk](../docs/xk/memory.md). After answer the following questions:

#### Question #3
Why does xk map kernel and user-application into the same address space? (Why does the kernel not have a separate address space?)

Please describe at least 2 reasons.

#### Question #4
> For both of the following questions we're looking for descriptions at the implementation level of how the user and kernel versions of the given functions differ. e.g.: "foo uses resource A, while bar uses resource B" is roughly the level of detail to go for here. We are not looking for an English transcription of the `git diff` output). We are looking for at least one difference for each function, but put down more if you identify them!

In what ways is the xk user malloc (`user/lib/umalloc.c:malloc`) different from the xk kernel malloc (`kernel/kalloc.c:kalloc`)?

In what ways is the xk user printf (`user/lib/printf.c:printf`) different from the xk kernel printf (`kernel/console.c:cprintf`)?

## Part 3: Kernel

### Starting the first user process

xk sets up the first user space program with `userinit` in `kernel/proc.c`. 
`userinit` first calls `allocproc` to allocate a process control block (`struct proc` in xk) for the new process.
`allocproc` sets up some initial process states in `struct proc` to ensure that the 
new process can be scheduled, and that the same struct will not be re-allocated while the process is still running.
Once a `struct proc` is allocated for the new process, `userinit` then initializes the virtual address space
of the new process, loads the init process's code, sets up its trapframe, and update the process state to runnable.
The trapframe is set up so that when it's scheduled, it can return to the user space and start execution.
It is important to understand how `userinit` works, as you will be doing something similar in the next lab.
After `userinit` finishes, the new process (`user/lab1test.c`) will be scheduled at some point, 
output a "hello world" message and hangs due to the first two lines of `lab1test.c:main`.
```c
int main() {
  printf(stdout, "hello world\n");
  while (1);

  if(open("console", O_RDWR) < 0){
    error("lab1test: failed to open the console");
  }
  dup(0);     // stdout
  dup(0);     // stderr

  // test code below
```
As you implement the system calls in the next part,
you should comment out the first two lines to run the tests.

## Part 4: System calls
A core purpose of an OS is to abstract hardware resources into
useful services for applications. And system calls are the interfaces for  OS services.

Currently xk supports only a few system calls:
- `kill(pid)`
  + kill a process with a given process id
- `sleep(n)`
  + sleep for n clock ticks
- `write(fd, buf, n)`
  + an incomplete implementation of write that outputs buf to the console

Your task is to implement the following list of system calls for the existing 
[xk file system](https://gitlab.cs.washington.edu/xk-public/24wi/-/blob/main/kernel/fs.c) 
so that applications can access it in a managed and safe way.
- `open(filename, mode)`
  + opens a file with the specified mode, returns a handle (file descriptor) for that open file
- `close(fd)`
  + closes the open file represented by the file descriptor
- `read(fd, buf, n)`
  + reads n bytes from an open file into buf
- `write(fd, buf, n)`
  + writes n bytes from buf into an an open file
- `dup(fd)`
  + duplicate a file descriptor, creating another handle for the same open file
- `fstat(fd, stat)`
  + populate stat with information of the open file

You will need to understand some basics of the xk file system to know what functions to call,
but mainly you will learn about how system calls are invoked, how arguments are passed, 
and how to protect the kernel from invalid and/or malicious system call arguments.

### The xk file system
The existing xk file system is a simple read-only file system.
A file system provides a namespace for persistent data (files and directories),
supports operations (read, write, truncate, etc) on the data,
and manages the storage device (allocate and deallocate disk blocks).
Each object in the file system has data and metadata, 
and in xk (and many other systems including linux), we use inodes for tracking the metadata.

In our file system, every file/directory and the console device has an inode associated to it, 
and the file system interface (`kernel/fs.c`) deals with inodes. 
For example, to open a file, you can call `iopen`, which performs a lookup from the
root directory of the file system. Once the file is found, a `struct inode` pointer
is returned as a handle so that you can request more file system operations with it.
For this lab, you will be using `iopen`, `irelease`, `concurrent_readi`, `concurrent_writei`, `idup`, and `concurrent_stati` to implement the system calls above.

As this point, you might wonder, how is there any data to read from if you can't write to files?
If you recall, when we run xk and QEMU, we give it an initial file system image `out/fs.img` written by [mkfs.c](https://gitlab.cs.washington.edu/xk-public/24wi/-/blob/main/mkfs.c).
`mkfs.c` runs on our normal linux machine and writes a number of user programs plus other files in a format that the xk file system understands.
Tada! Now our read only file system has data to read!

### System call invocation
Before you implement system calls, you need to first understand how system calls are invoked.

System call invocations are implemented with software interrupts. When a user application needs to make a system call, 
it first writes system call arguments into registers like a normal procedure call, but instead of a `call`
it issues an `int` instruction with the system call trap number to trigger a mode switch into the kernel. 
On x86_64 you can also just use the `syscall` instruction to invoke a system call. 

In xk, when you invoke a system call listed in [user/user.h](https://gitlab.cs.washington.edu/xk-public/24wi/-/blob/main/inc/user.h),
it goes to the provided system call stubs in [user/lib/usys.S](https://gitlab.cs.washington.edu/xk-public/24wi/-/blob/main/user/lib/usys.S), 
which writes the specific system call number (defined in `inc/syscall.h`) to the register `%eax` (line 7) and issues the software interrupt (line 8).

Once in kernel mode, the software interrupt is captured by the registered trap vector (`kernel/vectors.S`),
calls `alltraps`, which pushes the process's registers onto its kernel stack, and then lands at `kernel/trap.c:trap`. 
`trap` demux the interrupt to `kernel/syscall.c:syscall`, `syscall` then demuxes the call to the respective handler in `kernel/sysproc.c` or `kernel/sysfile.c`.

#### Question #5
What is the first line of c code executed in the kernel when there is an interrupt?
To force an interrupt, perform a system call. Add a `sleep` call within `lab1test.c` and use gdb to trace through it with the `si` command.
Our gdb script loads the kernel symbols on startup via the `kern` command, you would also want to load in the init program's symbols by using the `initcode` command in gdb.

#### Question #6
How large (in bytes) is a trap frame? (Hint: GDB's `print` command can print the value of arbitrary `C` expressions, including `sizeof` expressions. `print` is very powerful because of this).

#### Question #7
Set a breakpoint in the kernel implementation of a system call (e.g., `sys_sleep`) and continue
executing until the breakpoint is hit (be sure to call `sleep` within `lab1test.c`). 
Do a backtrace, `bt` in gdb. What kernel functions are reported by the backtrace when it reaches `sys_sleep`?

### System call arguments
Now that we are in a system call handler, how do we get the arguments? 

When a system call is invoked from user space, the system call arguments are written to different registers following the x86_64 calling convention.
Once inside the kernel, the value of all registers are pushed onto the kernel stack to free up the registers for the process to use once it resumes.
So we just need to read the saved registers to retrieve the corresponding arguments (see `kernel/syscall.c` for existing helper functions).

#### Question #8
Is it safe to use system call arguments without validation? (e.g.: consider when a user process calls `read`, and passes a pointer to a buffer to write to as an argument to `read`).

If yes, why? 
If not, how might things go wrong and what can the kernel do in response?

Responses should be roughly 2-3 sentences long.

*This is something you will have to consider when you implement your system calls.*

### Open file abstraction
By now you should know how to handle system call arguments and have a basic understanding of the available file system functions that you can use.
But there is still a missing piece - the open file abstraction. 
Notice there is a difference between the system call `open(filename, mode)` and the corresponding file system operation `iopen(filename)`.
This is because a user application may want to open the same file with different modes (read only, write only, read/write), requiring richer semantics than what the file system provides.

To support functionalities such as modes (`open`), duplication (`dup`), and implict advancement of file positions (`read`, `write`), 
you need to track these as part of the metadata (maybe a `struct file_info`) of an open file. 
Your open file metadata should track at least the following:
- the underlying inode
- current offset of the file (how far have we read) 
- access mode of the open file (identifies the file as readable, writable and so on)
- reference count of the struct 

Once you define your `struct file_info` in `kernel/file.c`, you also need to manage its allocation and deallocation.
There is a maximum number of open files per system that you need to support, so you can define a static array of `struct file_info` and manage that.
You can also declare corresponding file functions such as `file_read`, `file_write` that `sys_read` and `sys_write` can invoke after processing the arguments.
For any new functions you declare, you can add the declarations in `inc/defs.h`. 

### File descriptors
The last thing to consider is once the user process opens a file, what do we return back to the user?
We need to return a file handle so that user can continue to perform file system operation on it.
In unix systems this is called a file descriptor (fd). File descriptor is just an integer that is an identifier for an open file.
It is unique to each process, meaning that the same fd 4 can represent different open files for different processes.

You also need to manage the allocation and deallocation of file descriptors. 
There is a maximum number of open files (and hence available file descriptors) per process, 
so you can extend `struct proc` to contain a mapping of file descriptors to open files.
Make sure you keep the mapping updated for each open and close. Also, user process may or may not pass in valid file descriptors...

So far we've been looking at `open` `close` `dup` system calls as file system operations, 
but in fact, they can be done for other forms of I/O (console, socket, etc) as well. 
Despite the name "file descriptor", it is really an "I/O descriptor".
This lets applications to interact with all kinds of I/O using the same interface.
The console is simply a file (file descriptor) from the user application's point of view, 
and reading from keyboard and writing to screen is done through the same `read` `write` system calls.

#### Question #9
Another option for the file handle is the `struct file_info` pointer of the open file.

List one benefit and one harm of using `struct file_info` pointers as the file handles we return to user processes. Justify your answers (~1 sentence of justification for each listed item should be sufficient).

### System call specifications
For this lab, the TA solution makes changes to `sysfile.c`, `file.c`, `proc.h`, `defs.h` and `file.h`. You may change more or fewer files.

1) Open
```c
/*
 * arg0: char * [path to the file]
 * arg1: int [mode for opening the file (see inc/fcntl.h)]
 *
 * Given a pathname for a file, sys_open() returns a file descriptor, a small,
 * nonnegative integer for use in subsequent system calls. The file descriptor
 * returned by a successful call will be the lowest-numbered file descriptor
 * not currently open for the process.
 *
 * Each open file maintains a current position, initially zero.
 *
 * returns -1 on error
 *
 * Errors:
 * arg0 points to an invalid or unmapped address 
 * there is an invalid address before the end of the string 
 * the file does not exist
 * already at max open files
 * there is no available file descriptor 
 * since the file system is read only, any write flags for non console files are invalid (until lab 4)
 * O_CREATE is not permitted (until lab 4)
 *
 */
int
sys_open(void);
```

2) Dup
```c
/*
 * arg0: int [file descriptor]
 *
 * Duplicate the file descriptor arg0, must use the smallest unused file descriptor.
 * Return a new file descriptor of the duplicated file, -1 otherwise
 *
 * dup is generally used by the shell to configure stdin/stdout between
 * two programs connected by a pipe (lab 2).  For example, "ls | more"
 * creates two programs, ls and more, where the stdout of ls is sent
 * as the stdin of more.  The parent (shell) first creates a pipe 
 * creating two new open file descriptors, and then create the two children. 
 * Child processes inherit file descriptors, so each child process can 
 * use dup to install each end of the pipe as stdin or stdout, and then
 * close the pipe.
 *
 * Error conditions:
 * arg0 is not an open file descriptor
 * there is no available file descriptor
 */
int
sys_dup(void);
```

3) Close
```c
/*
 * arg0: int [file descriptor]
 *
 * Close the given file descriptor
 * Return 0 on successful close, -1 otherwise
 *
 * Error conditions:
 * arg0 is not an open file descriptor
 */
int
sys_close(void);
```

4) Read
```c
/*
 * arg0: int [file descriptor]
 * arg1: char * [buffer to write read bytes to]
 * arg2: int [number of bytes to read]
 *
 * Read up to arg2 bytes from the current position of the corresponding file of the 
 * arg0 file descriptor, place those bytes into the arg1 buffer.
 * The current position of the open file is then updated with the number of bytes read.
 *
 * Return the number of bytes read, or -1 if there was an error.
 *
 * Fewer than arg2 bytes might be read due to these conditions:
 * If the current position + arg2 is beyond the end of the file.
 * If this is a pipe or console device and fewer than arg2 bytes are available  (lab 2)
 * If this is a pipe and the other end of the pipe has been closed. (lab 2)
 *
 * Error conditions:
 * arg0 is not a file descriptor open for read 
 * some address between [arg1, arg1+arg2) is invalid
 * arg2 is not positive
 */
int
sys_read(void);
```

5) Write
```c
/*
 * arg0: int [file descriptor]
 * arg1: char * [buffer of bytes to write to the given fd]
 * arg2: int [number of bytes to write]
 *
 * Write up to arg2 bytes from arg1 to the current position of the corresponding file of
 * the file descriptor. The current position of the file is updated by the number of bytes written.
 *
 * Return the number of bytes written, or -1 if there was an error.
 *
 * If the full write cannot be completed, write as many as possible 
 * before returning with that number of bytes.
 *
 * If writing to a pipe and the other end of the pipe is closed,
 * return -1. (lab 2)
 *
 * Error conditions:
 * arg0 is not a file descriptor open for write
 * some address between [arg1,arg1+arg2-1] is invalid
 * arg2 is not positive
 *
 * note that for lab1, the file system does not support writing past 
 * the end of the file (or at all). Normally this would extend the size of the file
 * allowing the write to complete, to the maximum extent possible 
 * provided there is space on the disk.
 */
int
sys_write(void);
```

6) File Stat
```c
/*
 * arg0: int [file descriptor]
 * arg1: struct stat *
 *
 * Populate the struct stat pointer passed in to the function
 *
 * Return 0 on success, -1 otherwise
 *
 * Error conditions: 
 * if arg0 is not a valid file descriptor
 * if any address within the range [arg1, arg1+sizeof(struct stat)] is invalid
 */
int
sys_fstat(void);
```

#### Question #10
For each member of the project team, how many hours did you spend on this lab?

#### Question #11
What did you like or dislike about this lab? Is there anything you wish you knew earlier?

## Testing and hand-in
As you implement the system calls described above, you should comment out the first two lines of `lab1test.c:main` so the tests can run.
Run `make qemu` and you should see what tests you are currently passing.
Once all tests pass, you should see `passed lab1 tests!`.

Please submit your answers to the questions listed above to Gradescope.

To submit the code, make sure your repo is up-to-date, download it from GitLab using the download button, and submit it to Gradescope.
If you zip it yourself, make sure you run `make clean` first; the autograder fails if the upload is too large, which typically happens when you include the build outputs.
The top-level of the zip file should contain the Makefile, the `kernel/` folder and all other files/folders of the repo.

In case there is any problem with Gradescope, please also have a copy of your answer in `lab1.txt` in the top-level directory.

Lastly, add a git tag for your lab1 once it's done.
```
$ git tag end_lab1
$ git push origin main --tags
```
This will allow us to pull a version of your lab1 solution even when you start working on the next lab.
