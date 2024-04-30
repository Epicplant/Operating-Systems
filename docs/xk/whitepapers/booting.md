# Looking Into Booting

<!-- Originally by: tenzinhl -->

This not-so-organized post is a slew of notes around the x86 booting process.

## What Happens Before Main?

The question that inspired this investigation made a great point. The C standard requires multiple things to be setup by the time execution enters the main function (registers should have certain values, there should be a stack, code should be loaded and relocated into the correct locations, globals should be 0-initialized, etc.)! For user code typically this is done by a `_start` method which is provided by the C standard library (and automatically added to your program by the C compiler).

See this discussion from an embedded focused website on what happens before the `main` in user programs (but also relevant in general to assumptions all C-code has) with some extra details on what those expected things are etc.: [Link to Embedded Artistry Post](https://embeddedartistry.com/blog/2019/04/08/a-general-overview-of-what-happens-before-main/#:~:text=For%20most%20C%20and%20C%2B%2B,%2C%20compiler%2C%20and%20standard%20libraries).

You'll notice in the `xk` repo that there are more than just `*.c` files, and that there are also `*.S` assembly files that complement the C code.

At this point though, you might be wondering: okay, but how does `_start` get called (or whatever it is before we reach the kernel's `main`)?

## Some First Principles

As a reminder: a CPU is just a state machine that follows rules laid out by the Instruction Set Architecture (an ISA is actually a state machine specification, an abstract model of computing). Speaking in more grounded terms for x86: the CPU just takes instructions pointed to by `%rip` and executes them. This is our ground truth.

Physical memory can be viewed as a bus. The CPU just requests an address out on the bus, and in a proper setup only one device on the BUS owns that address and responds with the appropriate information (i.e.: physical memory can be composed of multiple devices backing that memory, e.g.: you probably have more than one RAM chip, but also things like the BIOS chip will own some address space in the physical memory space).

## Wait, So What Code Is Executed First?

Here by "first" we mean the first instruction executed after restarting the CPU. The way code is loaded into memory and how it should be stored is generally dependent on the computer you're running on (a computer is more than just a CPU, it's also the connected memory, flash, and other devices). e.g.: on many microcontrollers there's usually a special section of flash memory which you flash with your program, and it will literally just load that program starting to execute from whatever's at address 0.

From the perspective of an x86 chip, the first line of assembly executed after the reset signal is driven will be some hard-coded address determined by the CPU manufacturer ([LINK for source](https://cs.stackexchange.com/questions/63839/where-does-the-cpu-get-its-first-instructions-from), [LINK to what the "reset vectors" for various x86 CPUs are](https://en.wikipedia.org/wiki/Reset_vector#:~:text=by%20different%20microprocessors%3A-,x86%20family%20(Intel),maps%20to%20physical%20address%20FFFF0h.)). Then on the memory bus the device that owns this memory address will typically be some form of flash (persistent) memory that stores the BIOS. When we run make qemu we can see that the region `[mem 0xfffc0000-0xffffffff]` is reserved, (which is like ~262 kB below the 4GiB line. This contains the reset vector for x86_64 processors! If we wanted to see the first executed instruction we'd look at 0xffff fff0 in physical memory to see what instruction is there).

The BIOS then loads our bootloader from a fixed location on disk ([LINK](https://en.wikibooks.org/wiki/X86_Assembly/Bootloaders)), entering the `start` label in `bootasm.S`. From there we just follow normal assembly execution. Our bootloader handles the transition from real addressing mode to protected mode (allowing us access to larger part of memory space).

## x86 and Its Concepts

To clarify though: not all concepts are purely C concepts that need to be configured by bootloader etc., e.g.: the "stack" is a concept that's baked into the x86 ISA. The %rsp register is special use for tracking the top of the stack and is manipulated automatically as part of instructions like `call`, `push`, and `pop`.

## Further Reading

Going back to [the booting section in `overview.md` in the xk repo](../overview.md#booting) can be worthwhile, same with quickly checking out [`memory.md` (LINK)](../memory.md).

A book that we would HIGHLY recommend for its thorough discussion of x86 booting is ["Linux Inside" by 0xAX: LINK](https://0xax.gitbooks.io/linux-insides/content/Booting/). The section on booting is very in-depth and provides a great jumping board for terminology to use to Google-Fu you're way through any specific details you want to fill in.

There's a whole bunch of extra details there about the registers in real mode, some ways in which x86 subtly cheats (e.g.: the CS Base register has a value that couldn't normally be set in real mode when the processor resets), things like how the boot sector needs to be formatted... good fun if you're interested in really drilling down into it.
