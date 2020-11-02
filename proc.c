#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include <stddef.h>
struct {
	struct spinlock lock;
	struct proc proc[NPROC];
} ptable;

int cnt[] = {0, 0, 0, 0, 0};
int clkpr[] = {1, 2, 4, 8, 16};
int time_age = 50;
struct proc *q0[1000];
int beg0 = 0;
int end0 = 0;
struct proc *q1[1000];
int beg1 = 0;
int end1 = 0;
struct proc *q2[1000];
int beg2 = 0;
int end2 = 0;
struct proc *q3[1000];
int beg3 = 0;
int end3 = 0;
struct proc *q4[1000];
int beg4 = 0;
int end4 = 0;
static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
	initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
	return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
	int apicid, i;

	if (readeflags()&FL_IF)
		panic("mycpu called with interrupts enabled\n");

	apicid = lapicid();
	// APIC IDs are not guaranteed to be contiguous. Maybe we should have
	// a reverse map, or reserve a register to store &cpus[i].
	for (i = 0; i < ncpu; ++i) {
		if (cpus[i].apicid == apicid)
			return &cpus[i];
	}
	panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
	struct cpu *c;
	struct proc *p;
	pushcli();
	c = mycpu();
	p = c->proc;
	popcli();
	return p;
}
int aging() 
{
	acquire(&ptable.lock);
	
	for (struct proc *p = ptable.proc; p <= &ptable.proc[NPROC] - 1; p++) 
	{
		int tm;
		tm = ticks - p->agetime- p->rtime - p->ctime;
		if (p->state != RUNNABLE)
			continue;

		int no , ind ;
		if (time_age < tm )  
		{
			no = p->queuenumber;
			if (no == 1) 
			{
				ind = 0;
				for (int i = 0; i < cnt[1]; i++) 
				{
					if (p == q1[i]) 
					{
						ind = i;
						break;
					}
				}
				p->rrtime[0] = 0;
				p->queuenumber = 0;
				cnt[1]--;
				p->rrtime[1] = 0;
				for (int i = ind; i <= cnt[1]-1; i++) 
					q1[i] = q1[i + 1];

				int f = 0;
				for (int i = 0; i <= cnt[0]-1; i++) 
				{
					if (p==q0[i]) 
					{
						f = 1;
						break;
					}
				}
				if (!f) 
				{
					end0 ++;
					cnt[0]++;
					q0[cnt[0] - 1] = p;
				}
			} else if (no == 2) 
			{
				int ind = 0;
				for (int i = 0; i <= cnt[2]-1; i++) 
				{
					if (p->pid==q2[i]->pid) 
					{
						ind = i;
						break;
					}
				}
				p->rrtime[1] = 0;
				p->queuenumber = 1;
				cnt[2]--;
				p->rrtime[2] = 0;
				for (int i = ind; i <= cnt[2]-1; i++) 
					q2[i] = q2[i + 1];
				int f = 0;
				for (int i = 0; i <= cnt[1]-1; i++) 
				{
					if ( p->pid== q1[i]->pid ) 
					{
						f = 1;
						break;
					}
				}
				if (!f) 
				{
					end1 ++;
					cnt[1]++;
					q1[cnt[1] - 1] = p;
				}
			} else if (no == 3) 
			{
				int ind = 0;
				for (int i = 0; i <= cnt[3]-1; i++) 
				{
					if (p->pid == q3[i]->pid) 
					{
						ind = i;
						break;
					}
				}
				p->rrtime[2] = 0;
				p->queuenumber = 2;
				p->rrtime[3] = 0;
				cnt[3]--;
				for (int i = ind; i <= cnt[3]-1; i++) 
					q3[i] = q3[i + 1];
				int f = 0;
				for (int i = 0; i <= cnt[2]-1; i++) 
				{
					if (p->pid == q2[i]->pid) 
					{
						f = 1;
						break;
					}
				}
				if (!f) 
				{
					end2 ++;
					cnt[2]++;
					q2[cnt[2] - 1] = p;
				}
			} else if (no == 4) 
			{
				int ind = 0;
				for (int i = 0; i <= cnt[4]-1; i++) 
				{
					if (p->pid==q4[i]->pid) 
					{
						ind = i;
						break;
					}
				}
				p->rrtime[3] = 0;
				p->queuenumber = 3;
				p->rrtime[4] = 0;
				cnt[4]--;
				for (int i = ind; i <= cnt[4]-1; i++) 
					q4[i] = q4[i + 1];
				int f = 0;
				for (int i = 0; i <= cnt[3]-1; i++) 
				{
					if (p->pid==q3[i]->pid) 
					{
						f = 1;
						break;
					}
				}
				if (!f) 
				{
					end3 ++;
					cnt[3]++;
					q3[cnt[3] - 1] = p;
				}
			}
			p->agetime = ticks - p->rtime - p->ctime;
		}
	}
	release(&ptable.lock);
	return 0;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
	struct proc *p;
	char *sp;

	acquire(&ptable.lock);

	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if (p->state == UNUSED)
			goto found;

	release(&ptable.lock);
	return 0;

found:
	p->state = EMBRYO;
	p->pid = nextpid++;
	p->queuenumber = -1;
	p->stat.pid = p->pid;
	p->stat.current_queue = p->stat.num_run = p->stat.runtime = 0;
	p->priority = 60;
	for (int j = 0; j <= 4; j++) 
	{
		p->stat.ticks[j] = 0;
		p->rrtime[j] = 0;
	}
	// pushq(&q0, &p, &end0);
	q0[cnt[0]] = p;
	end0 ++;
	cnt[0]++;
	release(&ptable.lock);
	// Allocate kernel stack.
	if ((p->kstack = kalloc()) == 0) {
		p->state = UNUSED;
		return 0;
	}
	sp = p->kstack + KSTACKSIZE;

	// Leave room for trap frame.
	sp -= sizeof * p->tf;
	p->tf = (struct trapframe*)sp;

	// Set up new context to start executing at forkret,
	// which returns to trapret.
	sp -= 4;
	*(uint*)sp = (uint)trapret;

	sp -= sizeof * p->context;
	p->context = (struct context*)sp;
	memset(p->context, 0, sizeof * p->context);
	p->context->eip = (uint)forkret;
	p->ctime = ticks;
	p->rtime = p->etime = p->iotime = p->agetime = 0;
	return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
	struct proc *p;
	extern char _binary_initcode_start[], _binary_initcode_size[];

	p = allocproc();

	initproc = p;
	if ((p->pgdir = setupkvm()) == 0)
		panic("userinit: out of memory?");
	inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
	p->sz = PGSIZE;
	memset(p->tf, 0, sizeof(*p->tf));
	p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
	p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
	p->tf->es = p->tf->ds;
	p->tf->ss = p->tf->ds;
	p->tf->eflags = FL_IF;
	p->tf->esp = PGSIZE;
	p->tf->eip = 0;  // beginning of initcode.S

	safestrcpy(p->name, "initcode", sizeof(p->name));
	p->cwd = namei("/");

	// this assignment to p->state lets other cores
	// run this process. the acquire forces the above
	// writes to be visible, and the lock is also needed
	// because the assignment might not be atomic.
	acquire(&ptable.lock);

	p->state = RUNNABLE;

	release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
	uint sz;
	struct proc *curproc = myproc();

	sz = curproc->sz;
	if (n > 0) {
		if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
			return -1;
	} else if (n < 0) {
		if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
			return -1;
	}
	curproc->sz = sz;
	switchuvm(curproc);
	return 0;
}
unsigned short lfsr = 0xACE1u;
unsigned bit;
unsigned rand() {
	bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
	return lfsr = (lfsr >> 1) | (bit << 15);
}
int valuesToSet[] = {50, 40, 30, 20, 19, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
int inc = 0;
// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
	int i, pid;
	struct proc *np;
	struct proc *curproc = myproc();

	// Allocate process.
	if ((np = allocproc()) == 0) {
		return -1;
	}

	// Copy process state from proc.
	if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
		kfree(np->kstack);
		np->kstack = 0;
		np->state = UNUSED;
		return -1;
	}
	np->sz = curproc->sz;
	np->parent = curproc;
	*np->tf = *curproc->tf;

	// Clear %eax so that fork returns 0 in the child.
	np->tf->eax = 0;

	for (i = 0; i < NOFILE; i++)
		if (curproc->ofile[i])
			np->ofile[i] = filedup(curproc->ofile[i]);
	np->cwd = idup(curproc->cwd);

	safestrcpy(np->name, curproc->name, sizeof(curproc->name));

	pid = np->pid;
#ifdef PBS
	np->priority = pid >> 1;
#endif
#ifdef MLFQ
	np->queuenumber = 0;
#endif

	acquire(&ptable.lock);
	np->state = RUNNABLE;

	release(&ptable.lock);

	return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
	struct proc *curproc = myproc();
	struct proc *p;
	int fd;

	if (curproc == initproc)
		panic("init exiting");

	// Close all open files.
	for (fd = 0; fd < NOFILE; fd++) {
		if (curproc->ofile[fd]) {
			fileclose(curproc->ofile[fd]);
			curproc->ofile[fd] = 0;
		}
	}

	begin_op();
	iput(curproc->cwd);
	end_op();
	curproc->cwd = 0;

	acquire(&ptable.lock);

	// Parent might be sleeping in wait().
	wakeup1(curproc->parent);

	// Pass abandoned children to init.
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if (p->parent == curproc) {
			p->parent = initproc;
			if (p->state == ZOMBIE)
				wakeup1(initproc);
		}
	}
	curproc->etime = ticks;
	// Jump into the scheduler, never to return.
	curproc->state = ZOMBIE;
	sched();
	panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
	struct proc *p;
	int havekids, pid;
	struct proc *curproc = myproc();

	acquire(&ptable.lock);
	for (;;) {
		// Scan through table looking for exited children.
		havekids = 0;
		for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
			if (p->parent != curproc)
				continue;
			havekids = 1;
			if (p->state == ZOMBIE) {
				// Found one.
				pid = p->pid;
				kfree(p->kstack);
				p->kstack = 0;
				freevm(p->pgdir);
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->state = UNUSED;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if (!havekids || curproc->killed) {
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(curproc, &ptable.lock);  //DOC: wait-sleep
	}
}
int waitx(int* wtime, int* rtime)
{
	struct proc *p;
	int havekids, pid;
	struct proc *curproc = myproc();

	acquire(&ptable.lock);
	for (;;) {
		// Scan through table looking for exited children.
		havekids = 0;
		for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
			if (p->parent != curproc)
				continue;
			havekids = 1;
			if (p->state == ZOMBIE) {
				// Found one.
				int h = p->etime - p->iotime - p->ctime - p->rtime;
				*rtime = p->rtime;
				*wtime = h;
				// cprintf("%d %d %d\n", p->etime, p->ctime, p->rtime);
				pid = p->pid;
				kfree(p->kstack);
				p->kstack = 0;
				freevm(p->pgdir);
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->state = UNUSED;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if (!havekids || curproc->killed) {
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(curproc, &ptable.lock);  //DOC: wait-sleep
	}
}
//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
int check_priority(int prt) 
{
	acquire(&ptable.lock);
	
	for (struct proc *p = ptable.proc; p <= &ptable.proc[NPROC] - 1; p++) 
	{
		if (p->state != RUNNABLE)
			continue;
		if (  prt >= p->priority) 
		{
			release(&ptable.lock);
			return 1;
		}
	}
	release(&ptable.lock);
	return 0;
}
int set_priority(int new_priority, int pid) 
{
	
	int oprior = -1;  // in order to return old priority of the process
	acquire(&ptable.lock);
	for (struct proc *p = ptable.proc; p <= &ptable.proc[NPROC] - 1; p++) 
	{
		if (p !=0 ) 
		{
			if ( p->pid == pid)
			{
				int bb = p->priority;
				oprior = bb;
				p->priority = new_priority;
				cprintf("set priority of %d to %d\n", p->pid, p->priority);
				break;
			}
		}
	}
	release(&ptable.lock);
	return oprior;
}

void
scheduler(void)
{
#ifdef DEFAULT
	struct cpu *c = mycpu();
	c->proc = 0;
	struct proc* p;
	for (;;) {
		// Enable interrupts on this processor.
		sti();

		// Loop over process table looking for process to run.
		acquire(&ptable.lock);
		for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
			if (p->state != RUNNABLE)
				continue;
			// Switch to chosen process.  It is the process's job
			// to release ptable.lock and then reacquire it
			// before jumping back to us.
			c->proc = p;
			switchuvm(p);
			p->state = RUNNING;

			swtch(&(c->scheduler), p->context);
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->proc = 0;
		}
		release(&ptable.lock);

	}
// #else
#endif
#ifdef PBS
	struct cpu *c;
	struct proc* p;
	c = mycpu();
	c->proc = 0;
	for (;;) {
		// Enable interrupts on this processor.
		sti();

		// Loop over process table looking for process to run.
		acquire(&ptable.lock);
		int mprior = 101 , mprior2 = 101;
		// struct proc* maxpr = 0;
		// int count = 0;
		for (p = ptable.proc; p <= &ptable.proc[NPROC]-1; p++) 
		{
			if (p->state != RUNNABLE)
				continue;
			if ( mprior > p->priority)
				mprior = p->priority;
		}
		if (mprior != 101)
		{
			
		for (p = ptable.proc; p <= &ptable.proc[NPROC]-1; p++) 
		{
			mprior2 = 101;
			if (p->state != RUNNABLE)
				continue;
			if ( mprior == p->priority)
			{
				c->proc = p;

				switchuvm(p);
				p->state = RUNNING;

				swtch(&(c->scheduler), p->context);
				switchkvm();
				for (p = ptable.proc; p <= &ptable.proc[NPROC-1]; p++) 
				{
					if (p->state != RUNNABLE)
						continue;
					if (mprior2 > p->priority )
						mprior2 = p->priority;
				}
				if (mprior <= mprior2)
					c->proc = 0;
				else
					break;
			}
		}
		}
		else
		{
			release(&ptable.lock);
			continue;
		}
		
		release(&ptable.lock);

	}
// #else
#endif
#ifdef FCFS
	struct cpu *c;
	struct proc* p;
	c = mycpu();
	c->proc = 0;
	for (;;) {
		// Enable interrupts on this processor.
		sti();

		// Loop over process table looking for process to run.
		acquire(&ptable.lock);
		struct proc *minp;
		minp = 0;
		for (p = ptable.proc; p <= &ptable.proc[NPROC]-1; p++) 
		{
			if (p->state != RUNNABLE)
				continue;
			if (!minp )
			{
				minp = p;
				continue;
			}
			if (minp->ctime > p->ctime)
				minp = p;
		}
		if (minp)  
		{
			if(minp->state == RUNNABLE)
			{
			p = minp;
			c->proc = p;

			switchuvm(p);
			p->state = RUNNING;

			swtch(&(c->scheduler), p->context);
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->proc = 0;
			}
		}
		release(&ptable.lock);

	}
// #else
#endif
#ifdef MLFQ
	struct cpu *c;
	struct proc* p;
	c = mycpu();
	c->proc = 0;
	for (;;) {
		// Enable interrupts on this processor.
		sti();

		// Loop over process table looking for process to run.
		acquire(&ptable.lock);
		int procfound = 0;
		// first priority queue
		if (procfound == 0) 
		{
			for (int i = 0; i <= cnt[0]-1; i++) 
			{
				p = q0[i];
				if (p->state != RUNNABLE)
					continue;
				procfound = 1;
				beg0 = i;
				c->proc = p;
				switchuvm(p);
				p->state = RUNNING;
				p->stat.num_run+=1;
				swtch(&(c->scheduler), p->context);
				switchkvm();
				c->proc = 0;
				break;
			}
		}

		// second priority queu
		if (procfound == 0) 
		{
			for (int i = 0; i <= cnt[1]-1; i++) 
			{
				p = q1[i];
				if (p->state != RUNNABLE)
					continue;
				procfound = 1;
				beg1 = i;
				c->proc = p;
				switchuvm(p);
				p->state = RUNNING;
				p->stat.num_run+=1;
				swtch(&(c->scheduler), p->context);

				switchkvm();
				// Process is done running for now.
				// It should have changed its p->state before coming
				// back.
				c->proc = 0;
				break;
			}
		}

		// third priority queue
		if (procfound==0) 
		{
			for (int i = 0; i <= cnt[2]-1; i++) 
			{
				p = q2[i];

				if (p->state != RUNNABLE)
					continue;
				procfound = 1;
				beg2 = i;
				c->proc = p;
				switchuvm(p);
				p->state = RUNNING;
				p->stat.num_run+=1;
				swtch(&(c->scheduler), p->context);

				switchkvm();
				// Process is done running for now.
				// It should have changed its p->state before coming
				// back.
				c->proc = 0;
				break;
			}
		}

		// fourth priority queue
		if (procfound==0) 
		{
			for (int i = 0; i <= cnt[3]-1; i++) 
			{
				p = q3[i];
				if (p->state != RUNNABLE)
					continue;
				procfound = 1;
				beg3 = i;
				c->proc = p;
				switchuvm(p);
				p->state = RUNNING;
				p->stat.num_run+=1;
				swtch(&(c->scheduler), p->context);

				switchkvm();
				// Process is done running for now.
				// It should have changed its p->state before coming
				// back.
				c->proc = 0;
				break;
			}
		}

		// fifth priority queue
		if (procfound==0) 
		{
			for (int i = 0; i <= cnt[4]-1; i++) 
			{
				p = q4[i];
				if (p->state != RUNNABLE)
					continue;
				beg4 = i;
				c->proc = p;
				switchuvm(p);
				procfound = 1;
				p->state = RUNNING;
				p->stat.num_run+=1;
				swtch(&(c->scheduler), p->context);
				switchkvm();
				// Process is done running for now.
				// It should have changed its p->state before coming
				// back.
				c->proc = 0;
				break;
			}
		}
		release(&ptable.lock);
	}
// #else
#endif
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
	int intena;
	struct proc *p = myproc();

	if (!holding(&ptable.lock))
		panic("sched ptable.lock");
	if (mycpu()->ncli != 1)
		panic("sched locks");
	if (p->state == RUNNING)
		panic("sched running");
	if (readeflags()&FL_IF)
		panic("sched interruptible");
	intena = mycpu()->intena;
	swtch(&p->context, mycpu()->scheduler);
	mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
	acquire(&ptable.lock);  //DOC: yieldlock
	myproc()->state = RUNNABLE;
	sched();
	release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
	static int first = 1;
	// Still holding ptable.lock from scheduler.
	release(&ptable.lock);

	if (first) {
		// Some initialization functions must be run in the context
		// of a regular process (e.g., they call sleep), and thus cannot
		// be run from main().
		first = 0;
		iinit(ROOTDEV);
		initlog(ROOTDEV);
	}

	// Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock * lk)
{
	struct proc *p = myproc();

	if (p == 0)
		panic("sleep");

	if (lk == 0)
		panic("sleep without lk");

	// Must acquire ptable.lock in order to
	// change p->state and then call sched.
	// Once we hold ptable.lock, we can be
	// guaranteed that we won't miss any wakeup
	// (wakeup runs with ptable.lock locked),
	// so it's okay to release lk.
	if (lk != &ptable.lock) { //DOC: sleeplock0
		acquire(&ptable.lock);  //DOC: sleeplock1
		release(lk);
	}
	// Go to sleep.
	p->chan = chan;
	p->state = SLEEPING;

	sched();

	// Tidy up.
	p->chan = 0;

	// Reacquire original lock.
	if (lk != &ptable.lock) { //DOC: sleeplock2
		release(&ptable.lock);
		acquire(lk);
	}
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
	struct proc *p;

	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if (p->state == SLEEPING && p->chan == chan)
			p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
	acquire(&ptable.lock);
	wakeup1(chan);
	release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
	struct proc *p;

	acquire(&ptable.lock);
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if (p->pid == pid) {
			p->killed = 1;
			// Wake process from sleep if necessary.
			if (p->state == SLEEPING)
				p->state = RUNNABLE;
			release(&ptable.lock);
			return 0;
		}
	}
	release(&ptable.lock);
	return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
	static char *states[] = {[UNUSED]    "unused",[EMBRYO]    "embryo",[SLEEPING]  "sleep ",[RUNNABLE]  "runble",[RUNNING]   "run   ",[ZOMBIE]    "zombie"};
	int i;
	struct proc *p;
	char *state;
	uint pc[10];

	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) 
	{
		if (p->state == UNUSED)
			continue;
		if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
			state = states[p->state];
		else
			state = "???";
		cprintf("%d %s %s", p->pid, state, p->name);
		if (p->state == SLEEPING) 
		{
			getcallerpcs((uint*)p->context->ebp + 2, pc);
			for (i = 0; i < 10 && pc[i] != 0; i++)
				cprintf(" %p", pc[i]);
		}
		cprintf("\n");
	}
	
}
int ps(void)
{
	
	acquire(&ptable.lock);
	char* runstate[] = { "UNUSED",
						 "EMBRYO",
						 "SLEEPING",
						  "RUNNABLE",
						   "RUNNING",
						    "ZOMBIE" };
	cprintf("PID -- PRIORITY -- State --- r_time -- w_time -- n_run -- cur_q - q0 - q1 - q2 - q3 - q4\n");
	for (struct proc* p = ptable.proc; p <= &ptable.proc[NPROC] - 1; p++)
	{
		if (p->pid < 1)
			continue;

		cprintf("%d -- %d -- %s -- %d -- ", p->pid, p->priority, runstate[p->state], p->rtime);
		int wtime = p->etime - p->ctime - p->rtime - p->iotime;
		
		if (!p->etime)
			wtime = wtime + ticks;
		
		cprintf("%d -- %d -- %d -- ", wtime, p->stat.num_run, p->queuenumber);
		if (p->queuenumber != -1)
		{
			for (int i = 0; i <= 4; i++)
				cprintf("%d -- ", p->stat.ticks[i]);
		}
		else
		{
			for (int i = 0; i <= 4; i++)
				cprintf("0 -- ");
		}
		cprintf("\n");
	}
	release(&ptable.lock);
	return 0;
}