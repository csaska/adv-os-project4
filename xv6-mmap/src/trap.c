#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mman.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

int
pagefault_handler(struct trapframe *tf)
{
  struct proc* curproc = myproc();
  uint fault_addr = rcr2();

  cprintf("============in pagefault_handler============\n");
  cprintf("pid %d %s: trap %d err %d on cpu %d "
    "eip 0x%x addr 0x%x\n",
    curproc->pid, curproc->name, tf->trapno, tf->err, cpuid(), tf->eip, fault_addr);

  // If error code is a result of page violation
  if (tf->err & 0x1)
    return -1;

  // Find the region that contains our fault address or return
  struct mmregion *region = curproc->mmregion_head; 
  while (1) {
    if (!region)
      return -1;
    if (region->addr <= fault_addr && (region->addr + region->rsize) > fault_addr)
      break;
    region = region->rnext;
  }

  // Allocate a page of memory around fault address
  char *mem = kalloc();
  if(mem == 0) {
    cprintf("pagefault_handler out of memory\n");
    return -1;
  }
  memset(mem, 0, PGSIZE);

  int perm = PTE_U;
  if (region->prot == PROT_WRITE)
    perm |= PTE_W;

  if (mappages(curproc->pgdir, (char*)PGROUNDDOWN(fault_addr), PGSIZE, V2P(mem), perm) < 0)
    return -1;

  switchuvm(curproc);

  if (region->rtype == MAP_FILE) {
    if (fileseek(curproc->ofile[region->fd], region->offset) == -1)
      return -1;
    if (fileread(curproc->ofile[region->fd], mem, region->length) == -1)
      return -1;

    // TODO: clear dirty  bit
  }

  return 0;
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  // Attempt to handle pagefaults
  if (tf->trapno == T_PGFLT && pagefault_handler(tf) == 0)
    return;

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
