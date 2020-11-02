#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
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

  for (i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if (tf->trapno == T_SYSCALL) {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno) {
  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0) {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
#ifdef MLFQ
      aging();
#endif

      if (myproc()) 
      {
        if (myproc()->state == RUNNING) 
        {
          myproc()->stat.runtime+=1;
          myproc()->rtime+=1;
          myproc()->rrtime[myproc()->queuenumber]+=1;
          myproc()->stat.ticks[myproc()->queuenumber]+=1;
        } 
        else if (myproc()->state == SLEEPING) 
          myproc()->iotime+=1;
      }
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
    if (myproc() == 0 || (tf->cs & 3) == 0) {
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
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
#ifdef FCFS
// do nothing
#endif
#ifdef PBS
  if (myproc()!=0 && myproc()->state == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER) 
  {
    if (check_priority(myproc()->priority) == 1) 
      yield();
  }
#endif
#ifdef MLFQ
  if (myproc()!=0 && myproc()->state == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER) 
  {
    if (myproc()->rrtime[myproc()->queuenumber] != 0) 
    {
      if (!myproc()->queuenumber) 
      {
        int ind = -1;
        for (int i = 0; i <= cnt[0]-1; i++) 
        {
          if (myproc()->pid == q0[i]->pid) 
            ind = i;
        }
        myproc()->rrtime[0] = 0;
        myproc()->queuenumber = 1;
        myproc()->rrtime[1] = 0;
        cnt[0]-=1;
        for (int i = ind; i <= cnt[0]-1; i++) 
          q0[i] = q0[i + 1];

        int f = 0;
        for (int i = 0; i <= cnt[1]-1; i++) 
        {
          if (myproc()->pid == q1[i]->pid) 
          {
            f = 1;
            break;
          }
        }
        if ( !f ) 
        {
          end1 ++;
          cnt[1]+=1;
          q1[cnt[1] - 1] = myproc();
        }
        yield();
      } 
      else if (myproc()->queuenumber == 1 && myproc()->rrtime[1] % 2 == 0) 
      {
        int ind = 0;
        for (int i = 0; i <= cnt[1]-1; i++) 
        {
          if (myproc() == q1[i]) 
          {
            ind = i;
            break;
          }
        }
        myproc()->rrtime[1] = 0;
        myproc()->queuenumber = 2;
        myproc()->rrtime[2] = 0;
        cnt[1]-=1;
        for (int i = ind; i <= cnt[1]-1; i++) 
                  q1[i] = q1[i + 1];
        
        int f = 0;
        for (int i = 0; i <= cnt[2]-1; i++) 
        {
          if (myproc()->pid == q2[i]->pid) 
          {
            f = 1;
            break;
          }
        }
        if ( !f ) 
        {
          end2 ++;
          cnt[2]+=1;
          q2[cnt[2] - 1] = myproc();
        }
        yield();
      } 
      else if (myproc()->rrtime[2] % 4 == 0 && myproc()->queuenumber== 2 ) 
      {
        int ind = 0;
        for (int i = 0; i <= cnt[2]-1; i++) 
        {
          if (myproc() == q2[i]) 
          {
            ind = i;
            break;
          }
        }
        myproc()->rrtime[2] = 0;
        myproc()->queuenumber = 3;
        myproc()->rrtime[3] = 0;
        cnt[2]-=1;
        for (int i = ind; i <= cnt[2]-1; i++) 
          q2[i] = q2[i + 1];

        int f = 0;
        for (int i = 0; i <= cnt[3]-1; i++) 
        {
          if ( myproc()->pid == q3[i]->pid ) 
          {
            f = 1;
            break;
          }
        }
        if (!f) 
        {
          end3 ++;
          cnt[3]+=1;
          q3[cnt[3] - 1] = myproc();
        }
        yield();
      } 
      else if (myproc()->rrtime[3] % 8 == 0 && myproc()->queuenumber == 3 ) 
      {
        int ind = 0;
        for (int i = 0; i <= cnt[3]-1; i++) 
        {
          if ( myproc() == q3[i]) 
          {
            ind = i;
            break;
          }
        }
        myproc()->rrtime[3] = 0;
        myproc()->queuenumber = 4;
        myproc()->rrtime[4] = 0;
        cnt[3]-=1;
        for (int i = ind; i <= cnt[3]-1; i++) 
          q3[i] = q3[i + 1];
        int f = 0;
        for (int i = 0; i <= cnt[4]-1; i++) 
        {
          if (myproc()->pid == q4[i]->pid) 
          {
            f = 1;
            break;
          }
        }
        if (!f) 
        {
          end4 ++;
          cnt[4]+=1;
          q4[cnt[4] - 1] = myproc();
        }
        yield();
      } 
      else if (myproc()->rrtime[4] % 16 == 0 && myproc()->queuenumber == 4) 
      {
        int ind = 0;
        myproc()->rrtime[4] = 0;
        for (int i = 0; i <= cnt[4]-1; i++) 
        {
          if (myproc()== q4[i]) 
          {
            ind = i;
            break;
          }
        }
        end4 ++;
        q4[cnt[4]] = myproc();
        for (int i = ind; i <= cnt[4]-1; i++) 
          q4[i] = q4[i + 1];
        yield();
      } 
      else 
      {
 		int flag0 = 0;
        for (int j = 0; j <= cnt[0]-1; j++) 
        {
          if (q0[j]->state == RUNNABLE) 
          {
            flag0 = 1;
            break;
          }
        }
        int flag1 = 0;
        for (int j = 0; j <= cnt[1]-1; j++) 
        {
          if (q1[j]->state == RUNNABLE) 
          {
            flag1 = 1;
            break;
          }
        }
        int flag2 = 0;
        for (int j = 0; j <= cnt[2]-1; j++) 
        {
          if (q2[j]->state == RUNNABLE) 
          {
            flag2 = 1;
            break;
          }
        }
        int flag3 = 0;
        for (int j = 0; j <= cnt[3]-1; j++) 
        {
          if (q3[j]->state == RUNNABLE) 
          {
            flag3 = 1;
            break;
          }
        }

        if (myproc()->queuenumber == 1) 
        {
          if (flag0 == 1) 
            yield();
        } 
        else if (myproc()->queuenumber == 2) 
        {
          if (flag1 == 1 || flag0 == 1) 
            yield();
        } 
        else if (myproc()->queuenumber == 3) 
        {
          if (flag1 == 1 || flag2 == 1 || flag0 == 1) 
            yield();
        } 
        else if (myproc()->queuenumber == 4) 
        {
          if (flag1 == 1 || flag2 == 1 || flag3 == 1 || flag0 == 1) 
            yield();
        }
      }
    }
  }

#endif
#ifdef DEFAULT
  if (myproc()!=0 && myproc()->state == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER)
    yield();
#endif
  if (myproc()!=0 && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}
