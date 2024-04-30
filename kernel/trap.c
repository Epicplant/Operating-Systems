#include <cdefs.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <spinlock.h>
#include <trap.h>
#include <x86_64.h>

// Checks the bit of var at a given offset from the end
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

// Interrupt descriptor table (shared by all CPUs).
struct gate_desc idt[256];
extern void *vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

int num_page_faults = 0;

void tvinit(void) {
  int i;

  for (i = 0; i < 256; i++)
    set_gate_desc(&idt[i], 0, SEG_KCODE << 3, vectors[i], KERNEL_PL);
  set_gate_desc(&idt[TRAP_SYSCALL], 1, SEG_KCODE << 3, vectors[TRAP_SYSCALL],
                USER_PL);

  initlock(&tickslock, "time");
}

void idtinit(void) { lidt((void *)idt, sizeof(idt)); }

void trap(struct trap_frame *tf) {
  uint64_t addr;

  if (tf->trapno == TRAP_SYSCALL) {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno) {
  case TRAP_IRQ0 + IRQ_TIMER:
    if (cpunum() == 0) {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case TRAP_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + 7:
  case TRAP_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n", cpunum(), tf->cs, tf->rip);
    lapiceoi();
    break;

  default:
    addr = rcr2();

    if (tf->trapno == TRAP_PF) {
      num_page_faults += 1;

      // LAB3: page fault handling logic here
      uint64_t stack_base = myproc()->vspace.regions[VR_USTACK].va_base;
      uint64_t max_stack_size = stack_base - (PAGE_SIZE * 10);

      // Check to see if the fault occurred because we tried to grow the stack
      if((addr <= stack_base && addr >= max_stack_size)
         && !CHECK_BIT(tf->err, 0)) {

        uint64_t stack_top = stack_base - myproc()->vspace.regions[VR_USTACK].size;
      
        int aligned_addr = PGROUNDDOWN(addr);

        int amt = vregionaddmap(&myproc()->vspace.regions[VR_USTACK], aligned_addr, stack_top - aligned_addr, VPI_PRESENT, VPI_WRITABLE);
        if(amt < 0) {
          goto error;
        }
          
        myproc()->vspace.regions[VR_USTACK].size += amt;
       
        vspaceupdate(&myproc()->vspace);
 
        return;
      }


      // Check to see if the fault occurred because we were writing to a read-only field
      // and that read-only field was copy on write
      

      struct vregion* test = va2vregion(&myproc()->vspace, addr);
      if(test == NULL) {
        goto error;
      }

      struct vpage_info* page = va2vpage_info(test, addr);
      if(page == NULL) {
        goto error;
      }


      struct core_map_entry* old_frame = (struct core_map_entry *) pa2page(page->ppn << PT_SHIFT);

      if(page->writable != VPI_WRITABLE  && page->cow == true
         && CHECK_BIT(tf->err, 1) && CHECK_BIT(tf->err, 0)
         && old_frame->reference >= 1) {

    
        if(old_frame->reference > 1) {
          // For the copy on write, we will first create a new frame for us to be able to write too using kalloc.
          // We’ll copyover the data that the page originally pointed too into a new frame, and then we’ll point this page to that frame.
           
          char* new_addr = kalloc();
          if(new_addr == NULL) {
            goto error;
          }

          // Construct the new frame;
          memmove(new_addr, P2V(page->ppn << PT_SHIFT), PGSIZE);

          kfree(P2V(page->ppn << PT_SHIFT));
        
          // Have the virtual page point to the new frame 
          struct core_map_entry* test = pa2page(V2P(new_addr));
          acquirekmem();
          test->va = old_frame->va;
          test->user = old_frame->user;
          releasekmem();
          page->ppn = PGNUM(V2P(new_addr));
        } 

        page->writable = VPI_WRITABLE;
        page->cow = false;

        
        vspaceupdate(&myproc()->vspace);
        vspaceinstall(myproc());

        return;
      }

    error: 

      if (myproc() == 0 || (tf->cs & 3) == 0) {
        // In kernel, it must be our mistake.
        cprintf("unexpected trap %d err %d from cpu %d rip %lx (cr2=0x%x)\n",
                tf->trapno, tf->err, cpunum(), tf->rip, addr);
        panic("trap");
      }
    }

    // Assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "rip 0x%lx (cr2=0x%x)--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno, tf->err, cpunum(),
            tf->rip, addr);
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING &&
      tf->trapno == TRAP_IRQ0 + IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}
