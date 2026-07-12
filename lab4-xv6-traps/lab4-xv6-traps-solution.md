# Lab 4 Report

[English](./lab4-xv6-traps-solution.md) · [中文](./lab4-xv6-traps-solution-zh.md)

**Overview:** Understand trap-based system call flow, implement kernel backtrace, and add periodic user-level alarms (`sigalarm`/`sigreturn`).

**Difficulty:** `risc-v assembly` (easy) · `backtrace` (moderate) · `alarm` (hard)

## RISC-V assembly (easy)

**1. Which registers hold function arguments? For example, when `main` calls `printf`, which register stores argument `13`?**

General-purpose registers are `x0` to `x31` (32 total). Among them, `a0` to `a7` are used for function arguments.

```asm
  2a:	0800                	addi	s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  2c:	4635                	li	a2,13
  2e:	45b1                	li	a1,12
```

From the disassembly, `13` is stored in register `a2`.

From source-level analysis, `printf` has three arguments:

```c
printf("%d %d\n", f(8)+1, 13);
```

`a0` holds the pointer to `"%d %d\n"`, `a1` holds `f(8)+1`, and `a2` holds `13`, which matches the disassembly.

**2. In `main`'s assembly, where is the call to function `f`? Where is the call to function `g`? (Hint: the compiler may inline functions.)**

A global search for `jal` or `call` instructions shows no explicit call to `f` or `g`.

Compare source and disassembly:

```c
int g(int x) {
  return x+3;
}

int f(int x) {
  return g(x);
}
```

```asm
int g(int x) {
   0:	1141                	addi	sp,sp,-16
   2:	e406                	sd	ra,8(sp)
   4:	e022                	sd	s0,0(sp)
   6:	0800                	addi	s0,sp,16
  return x+3;
}
   8:	250d                	addiw	a0,a0,3
   a:	60a2                	ld	ra,8(sp)
   c:	6402                	ld	s0,0(sp)
   e:	0141                	addi	sp,sp,16
  10:	8082                	ret

0000000000000012 <f>:

int f(int x) {
  12:	1141                	addi	sp,sp,-16
  14:	e406                	sd	ra,8(sp)
  16:	e022                	sd	s0,0(sp)
  18:	0800                	addi	s0,sp,16
  return g(x);
}
  1a:	250d                	addiw	a0,a0,3
  1c:	60a2                	ld	ra,8(sp)
  1e:	6402                	ld	s0,0(sp)
  20:	0141                	addi	sp,sp,16
  22:	8082                	ret
```

Function `f` does not call `g` at runtime; the compiler inlined `g` (`x+3`) directly into `f`.  
Likewise, `f` is inlined in `main`, so there is no visible `jal`/`call` for `f` either.

**3. At what address is `printf` located?**

```asm
 710:	4981                	li	s3,0
 712:	b359                	j	498 <vprintf+0x44>

0000000000000714 <fprintf>:

void
fprintf(int fd, const char *fmt, ...)
{
 714:	715d                	addi	sp,sp,-80
 716:	ec06                	sd	ra,24(sp)
 718:	e822                	sd	s0,16(sp)
```

By global search in disassembly, the address for `printf` is `0x714`.

**4. After the `jal` to `printf` in `main`, what is the value in register `ra`?**

```asm
0000000000000024 <main>:

void main(void) {
  24:	1141                	addi	sp,sp,-16
  26:	e406                	sd	ra,8(sp)
  28:	e022                	sd	s0,0(sp)
  2a:	0800                	addi	s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  2c:	4635                	li	a2,13
  2e:	45b1                	li	a1,12
  30:	00001517          	auipc	a0,0x1
  34:	8c050513          	addi	a0,a0,-1856 # 8f0 <malloc+0xfa>
  38:	706000ef          	jal	73e <printf>
  exit(0);
  3c:	4501                	li	a0,0
  3e:	2ba000ef          	jal	2f8 <exit>
```

`jal` writes the return address (the next instruction address) to `ra`, so `ra = 0x3c`.

**5. What is the output of the following code?**

```c
unsigned int i = 0x00646c72;
printf("H%x Wo%s", 57616, (char *) &i);
```

Running after modifying `call.c` gives:

```sh
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ call
HE110 World$ 
```

`57616` in hex is `0xe110`, so `%x` prints `e110`.  
`i = 0x00646c72`; on little-endian RISC-V, bytes in memory are `0x72 0x6c 0x64 0x00`, i.e. `"rld\0"`, so `%s` prints `rld`.

Combined output is `Hello World`.

On a big-endian machine, reverse byte order to get equivalent behavior, e.g. `i = 0x726c6400`.

**6. In `printf("x=%d y=%d", 3);`, what is printed after `y=`?**

Only one integer argument (`3`) is provided. The second `%d` tries to read another argument (effectively from whatever value is currently in the corresponding argument register/stack slot). That value is garbage, so output after `y=` is undefined behavior.

## Backtrace (moderate)

Following the lab instructions, first add declarations/configuration in `kernel/defs.h` and `kernel/riscv.h`.

Add in `kernel/defs.h`:

```c
int             printf(char*, ...) __attribute__ ((format (printf, 1, 2)));
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);
void            backtrace(void);
```

Then add the provided helper in `kernel/riscv.h` near the top:

```h
#ifndef __ASSEMBLER__

static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}
```

`backtrace` uses this helper to read frame pointer `s0`.

> To help with backtraces, the compiler generates machine code that maintains a stack frame on the stack corresponding to each function in the current call chain. Each stack frame consists of the return address and a "frame pointer" to the caller's stack frame.

Core logic: get current frame pointer via `r_fp()`. Each frame contains return address and previous frame pointer.

> Register s0 contains a pointer to the current stack frame (it actually points to the the address of the saved return address on the stack plus 8).

So `fp-8` gives saved return address, `fp-16` gives previous frame pointer. Use `PGROUNDUP(fp)` as stack top boundary, and walk frame-by-frame until top/end.

```c
void
backtrace(void)
{
  uint64 fp = r_fp();
  // calculate the top of the stack
  uint64 top = PGROUNDUP(fp);

  printf("backtrace:\n");
  while(fp < top){
    uint64 ra = *(uint64*)(fp - 8);
    printf("%p\n", (void*)ra);
    // to find out the fp for the invoker
    // this means to follow the invoke chain to print ra from the entire stack
    uint64 next_fp = *(uint64*)(fp - 16);
    // if beyond the stack top or not exist, just break
    if (next_fp >= top || next_fp == 0)
      break;
    fp = next_fp;
  }
}
```

Finally, call `backtrace` in `panic` and `sys_pause`:

```c
void
panic(char *s)
{
  panicking = 1;
  printf("panic: ");
  printf("%s\n", s);
  backtrace(); // print the backtrace info
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
```

```c
uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  backtrace(); 

  argint(0, &n);
  if(n < 0)
    n = 0;
```

## Alarm (hard)

Following the lab hints, first complete syscall/user-space plumbing.

Update Makefile to include `alarmtest`:

```
UPROGS += \
	$U/_call\
	$U/_bttest\
	$U/_alarmtest
endif
```

Add declarations in `user/user.h`:

```h
// umalloc.c
void* malloc(uint);
void free(void*);

// alarm
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```

Add syscall entries in `user/usys.pl`:

```pl
entry("sigalarm");
entry("sigreturn");
```

Then update `kernel/syscall.h` and `kernel/syscall.c`:

```h
#define SYS_mkdir      20
#define SYS_close      21
#define SYS_sigalarm   22
#define SYS_sigreturn  23 
```

```c
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_sigalarm(void);
extern uint64 sys_sigreturn(void);

[SYS_mkdir]       sys_mkdir,
[SYS_close]       sys_close,
[SYS_sigalarm]    sys_sigalarm,
[SYS_sigreturn]   sys_sigreturn,
```

Add initial stubs in `kernel/sysproc.c`:

```c
uint64
sys_sigalarm(void)
{
  return 0;
}

uint64
sys_sigreturn(void)
{
  return 0;
}
```

After scaffolding is complete, implement core alarm logic.

`sigalarm` sets an interval (ticks) and handler function address.  
When timer interrupts reach that interval, trap handling should jump to handler.  
When handler finishes, it calls `sigreturn` so execution can resume from interrupted context.

> Every tick, the hardware clock forces an interrupt, which is handled in usertrap() in kernel/trap.c.

At each timer interrupt, update tick count; when reaching interval, save full trapframe and replace `epc` with handler address. On return to user mode, execution enters handler. `sigreturn` restores saved trapframe and returns to the original code path.

> Prevent re-entrant calls to the handler----if a handler hasn't returned yet, the kernel shouldn't call it again. test2 tests this.

So we need an extra process-state flag indicating whether handler is currently running.

Add fields to `struct proc`:

```c
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  // alarm/interval
  struct trapframe *save;
  int ticks;
  int interval;
  int alarm_state;
  void *handler;
};
```

`save` stores a full trapframe snapshot; `ticks` counts elapsed ticks; `interval` stores user setting; `handler` stores handler address; `alarm_state` tracks handler running state.

Then initialize/free these fields in `allocproc`/`freeproc`:

```c
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Allocate a trapframe page.
  if((p->save = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // init the value for alarm
  p->ticks = 0;
  p->interval = 0;
  p->handler = 0;
  p->alarm_state = 0;

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->save)
    kfree((void*)p->save);
  p->save = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  p->ticks = 0;
  p->interval = 0;
  p->handler = 0;
  p->alarm_state = 0;
}
```

Implement `sys_sigalarm` to persist interval and handler:

```c
uint64
sys_sigalarm(void)
{
  int n;
  uint64 h;

  argint(0, &n); // get interval
  argaddr(1, &h); // get handler

  struct proc *p = myproc();
  p->ticks = 0; // reset the ticks
  p->interval = n;
  p->handler = (void*)h;

  return 0;
}
```

Update `usertrap`:

```c
uint64
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);  //DOC: kernelvec

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(killed(p))
      kexit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if((r_scause() == 15 || r_scause() == 13) &&
            vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
    // page fault on lazily-allocated page
  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    kexit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2){
    if(p->interval != 0){
      p->ticks = p->ticks + 1;
      // achieve the interval & the alarm function is not running
      if(p->ticks >= p->interval && p->alarm_state == 0){
        p->ticks = 0;
        // save the entire trapframe
        *p->save = *p->trapframe;
        p->trapframe->epc = (uint64)p->handler;
        p->alarm_state = 1;
      }
    }
    yield();
  }

  prepare_return();

  // the user page table to switch to, for trampoline.S
  uint64 satp = MAKE_SATP(p->pagetable);

  // return to trampoline.S; satp value in a0.
  return satp;
}
```

The new logic is in `if(which_dev == 2)`: when timer interrupt arrives, increment `ticks`; when interval is reached and no handler is currently running, reset ticks, snapshot trapframe to `save`, rewrite `epc` to handler, and mark `alarm_state=1`.

After handler executes, `sys_sigreturn` clears state and restores trapframe:

```c
uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();

  p->alarm_state = 0;
  *p->trapframe = *p->save;

  return p->trapframe->a0;
}
```

> Make sure to restore a0. sigreturn is a system call, and its return value is stored in a0.

So we directly return restored `a0` from trapframe.

## Lab 4 Full Test

After completing all parts, run full Lab 4 grading:

```sh
./grade-lab-traps
```

![2026-07-10 19.36.21](./2026-07-10%2019.36.21.png)
