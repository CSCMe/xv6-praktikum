#include "memlayout.h"
#include "defs.h"
#include "scauses.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

// scauses and their names according to
// https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf
// AMO = Atomic Memory Operation
// Not mapped: Interrupt causes
static char* scause_map[] = {
[SCAUSE_IAM]        "Instruction address misaligned",
[SCAUSE_IAF]          "Instruction address fault",
[SCAUSE_II]           "Illegal instruction",
[SCAUSE_BR]           "Breakpoint",
[SCAUSE_RESERVED_1]   "Reserved",
[SCAUSE_LAF]          "Load access fault",
[SCAUSE_AAM]          "AMO address misalignd",
[SCAUSE_ST_AMO_AF]    "Store/AMO access fault",
[SCAUSE_ECALL]        "Environment call",
[SCAUSE_RESERVED_2]   "Reserved",
[SCAUSE_IPF]          "Instruction page fault",
[SCAUSE_LOAD_PF]      "Load page fault",
[SCAUSE_RESERVED_3]   "Reserved",
[SCAUSE_ST_AMO_PF]    "Store/AMO page fault",
};

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();

  uint64 scause = r_scause();
  if(scause == SCAUSE_ECALL){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if (scause == SCAUSE_LOAD_PF || scause == SCAUSE_ST_AMO_PF) { // If load or store failed, try to recover
    //pr_debug("usertrap(): scause LOAD/STORE page fault. pid=%d\n", p->pid);
    //pr_debug("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    int recovery_failed = populate_mmap_page(r_stval());
    if (recovery_failed) {
      uint64 failed_addr = r_stval();
      pr_warning("\nusertrap(): unrecoverable LOAD/STORE page fault: pid=%d\n", p->pid);
      pr_warning("            %s\n", scause_map[scause]);
      pr_warning("            sepc=%p stval=%p\n", r_sepc(), failed_addr);
      pr_warning("            Page: %d.%d.%d\n", PX(2, failed_addr), PX(1, failed_addr), PX(0, failed_addr));
      setkilled(p);
    }
  } else {
    pr_warning("\nusertrap(): unexpected scause %p pid=%d\n", scause, p->pid);
    if (scause < 16) {
      pr_warning("            %s\n", scause_map[scause]);
    }
    pr_warning("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    pr_emerg("scause %p\n", scause);
    if (scause < 16) {
      pr_emerg("%s\n", scause_map[scause]);
    }
    pr_emerg("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if (irq == VIRTIO1_IRQ){
      virtio_net_intr();
    } else if(irq){
      pr_notice("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

