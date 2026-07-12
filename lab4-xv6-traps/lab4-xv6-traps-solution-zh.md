# Lab 4 实验报告

[English](./lab4-xv6-traps-solution.md) · [中文](./lab4-xv6-traps-solution-zh.md)

**目标简述：** 理解基于 trap 的系统调用流程，实现内核 backtrace，并加入周期性用户态 alarm（`sigalarm`/`sigreturn`）机制。

**实验难度：** `risc-v assembly` (easy) · `backtrace` (moderate) · `alarm` (hard)

## RISC-V assembly (easy)

**1. 哪些寄存器存储着函数的参数？例如，在 `main` 函数调用 `printf` 时，哪个寄存器保存着参数 13？**

通用寄存器为 `x0` 到 `x31`，共 32 个，其中 8 个（`a0` 至 `a7`）用于保存函数参数。

```asm
  2a:	0800                	addi	s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  2c:	4635                	li	a2,13
  2e:	45b1                	li	a1,12
```

查看反汇编代码可知，13 被存放在 `a2` 寄存器中。

结合源码分析，`printf` 共有三个参数：

```c
printf("%d %d\n", f(8)+1, 13);
```

`a0` 寄存器保存指向字符串 `"%d %d\n"` 的指针，`a1` 保存 `f(8)+1` 的结果，`a2` 保存 13，这与反汇编结果一致。

**2. `main` 函数的汇编代码中，函数 `f` 的调用在哪里？函数 `g` 的调用在哪里？（提示：编译器可能会内联函数。）**

全局搜索 `jal` 或 `call` 指令，未发现对函数 `f` 和函数 `g` 的调用。

对照源码与反汇编代码进行分析：

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

可以看到函数 `f` 内部并未调用函数 `g`，编译器直接将 `g` 内联展开，把 `x+3` 的操作写入了函数 `f` 中后返回；函数 `f` 与函数 `g` 的汇编代码完全一致。这是因为函数 `f` 仅仅调用了函数 `g`，本身没有其他操作，因此编译器选择将其内联。

同理，`main` 函数也直接内联了函数 `f`，所以反汇编代码中看不到 `jal` 或 `call` 指令。

**3. `printf` 函数位于哪个地址？**

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

通过全局搜索可以定位到，`printf` 函数地址为 `0x714`。

**4. `main` 函数中 `printf` 函数调用 `jal` 之后，寄存器 `ra` 的值是多少？**

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

由于 `jal` 指令会自动将下一条待执行指令的地址写入 `ra` 寄存器，以便后续返回时使用，因此 `ra` 寄存器的值为 `0x3c`。

**5. 运行以下代码的输出结果：**

```c
unsigned int i = 0x00646c72;
printf("H%x Wo%s", 57616, (char *) &i);
```

直接修改 `call.c` 后运行，会得到如下结果：

```sh
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ call
HE110 World$ 
```

57616 换算为十六进制为 `0xe110`，因此 `%x` 会输出 `e110`。

变量 `i` 的值为 `0x00646c72`。由于 RISC-V 采用小端（little-endian）存储方式，内存中（从低地址到高地址）的字节顺序依次为 `0x72`、`0x6c`、`0x64`、`0x00`，对应的 ASCII 字符分别为 `r`、`l`、`d` 以及字符串结束符 `\0`。因此 `%s` 打印 `&i` 得到的结果是 `rld`。

两部分拼接后即为 `Hello World`。

在大端机器上，最高有效字节存放在最低地址（与小端顺序相反），只需将字节顺序反转即可，即 `i = 0x726c6400`。

**6. `printf("x=%d y=%d", 3);` 中 `y=` 后面会打印什么？**

由于调用时仅传入了字符串和一个整数 3，`%d` 对应的第二个参数会尝试读取 `a2` 寄存器中的值并打印在 `y=` 之后。由于该寄存器并未被显式赋值，其中保留的是上一次调用遗留下来的垃圾值，因此打印结果没有实际意义，属于未定义行为（undefined behavior）。这与早期学习 C 语言时忘记传递参数导致输出垃圾值的现象类似。

## Backtrace (moderate)

根据实验提示，首先在 `kernel/defs.h` 和 `kernel/riscv.h` 中添加相应的函数声明和配置。

在 `kernel/defs.h` 中添加：

```c
int             printf(char*, ...) __attribute__ ((format (printf, 1, 2)));
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);
void            backtrace(void);
```

随后将实验提示中给出的代码复制到 `kernel/riscv.h`，放置在文件开头：

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

在 `backtrace` 函数中调用该函数以读取当前帧指针，`r_fp()` 通过内联汇编读取寄存器 `s0` 的值。

> To help with backtraces, the compiler generates machine code that maintains a stack frame on the stack corresponding to each function in the current call chain. Each stack frame consists of the return address and a "frame pointer" to the caller's stack frame.

随后开始实现核心的 `backtrace` 函数：通过 `r_fp()` 获取当前帧指针。根据提示可知，每个栈帧由两部分构成——返回地址，以及指向调用者栈帧的帧指针。

> Register s0 contains a pointer to the current stack frame (it actually points to the the address of the saved return address on the stack plus 8).

根据上述说明可知，当前帧指针减去 8 即为返回地址所在位置，减去 16 则到达上一个完整的栈帧。利用 `PGROUNDUP` 计算出栈顶地址，随后沿栈帧链依次向上查找，直到到达栈顶或链条终止为止。

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

最后需要在 `panic` 和 `sys_pause` 函数中加入 `backtrace` 调用：

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

根据题目提示，首先完成相关配置和调用的准备工作。

在 Makefile 中更新配置，将 `alarmtest` 加入编译列表，使 `alarmtest.c` 被编译为 xv6 用户程序：

```
UPROGS += \
	$U/_call\
	$U/_bttest\
	$U/_alarmtest
endif
```

随后在 `user/user.h` 中添加相应的函数声明：

```h
// umalloc.c
void* malloc(uint);
void free(void*);

// alarm
int sigalarm(int ticks, void (*handler)());
int sigreturn(void); No newline at end of file
```

再在 `user/usys.pl` 文件末尾添加系统调用入口：

```pl
entry("sigalarm");
entry("sigreturn");
```

接下来进入系统调用部分，修改 `kernel/syscall.h` 与 `kernel/syscall.c`，完成两个函数的初始化，使其能够被正确调用。

此处将 22 和 23 分配给这两个系统调用作为调用号：

```h
#define SYS_mkdir      20
#define SYS_close      21
#define SYS_sigalarm   22
#define SYS_sigreturn  23 
```

在 `kernel/syscall.c` 中导出这两个函数，并建立系统调用号表：

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

最后实现这两个函数的初始化逻辑。由于该功能与进程（`proc`）关系更为密切，因此将其放置在 `kernel/sysproc.c` 中：

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

至此，初始化阶段的准备工作已经完成，接下来进入核心逻辑的实现。

在实现具体逻辑之前，先整理整体思路：定时器通过 `sigalarm` 设定两个参数——一个是 tick 数（即 interval，表示经过多少个 tick 之后调用 handler），另一个是 handler（即定时调用的处理函数）。需要注意的是，该 handler 在执行结束时会调用 `sigreturn`，用以表示定时器处理已经完成，可以返回到原先被中断的函数继续执行。

> Every tick, the hardware clock forces an interrupt, which is handled in usertrap() in kernel/trap.c.

当 tick 数达到设定值后，会触发一次中断，系统进入内核态，并在 `usertrap` 中完成相应处理。处理完成后，`trapframe` 中保存的寄存器值会被恢复到 CPU 寄存器中，其中关键的一步是将 `epc` 的值恢复到真正的 PC 寄存器，随后执行 `sret` 指令——CPU 会跳转到 PC 所指向的地址继续执行，如同中断从未发生过一样。

可以在此处进行修改：每次系统时钟更新时，将 `ticks` 加一，并判断是否已到达设定时间。若已到达，则重置 `ticks` 计数，保存当前状态的副本，并篡改 `epc`，将其改写为 handler 函数的地址。这样系统恢复执行时会自动跳转到 handler 执行；待 handler 执行完毕并调用 `sigreturn` 后，再将此前保存的状态恢复，跳转回原函数继续运行。

> Prevent re-entrant calls to the handler----if a handler hasn't returned yet, the kernel shouldn't call it again. test2 tests this.

当 handler 正在运行期间，不应再次调用它，因此需要额外增加一个状态字段来记录 handler 的运行状态。

思路理清后，开始对 `proc` 的数据结构进行改造：

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

新增字段 `save`，用于保存完整的 `trapframe` 副本；`ticks` 用于记录已经经过的 tick 数；`interval` 为用户设定的定时周期；`handler` 为处理函数的地址指针；`alarm_state` 用于记录 handler 的运行状态。

结构体修改完成后，需要相应更新 `proc` 的初始化与释放逻辑：新增字段统一初始化为 0，`save` 的内存分配方式与 `trapframe` 相同。

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

接下来实现 `sys_sigalarm` 的逻辑，将用户传入的 tick 数和 handler 地址分别写入进程结构体中的 `interval` 和 `handler` 字段。要注意的是，应使用 `argint` 和 `argaddr` 这两个函数从系统调用中获取用户传入的参数：

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

随后更新 `usertrap` 函数：

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

新增代码位于 `if(which_dev == 2)` 分支下，具体逻辑如下：首先判断是否设置了 `interval`；若已设置，则在每次时钟中断到达时更新 `ticks` 计数。当 `ticks` 达到设定值，且当前 alarm/handler 未处于运行状态时，先重置 `ticks`，随后将完整的 `trapframe` 保存至 `save`，最后篡改 `epc` 为 handler 地址并更新 `alarm_state`，使程序自动跳转至 handler 执行。

待 handler 执行完毕后会调用 `sys_sigreturn`，重置 alarm 状态，并将此前保存的 `trapframe` 恢复回去：

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

返回值直接取自恢复后的 `trapframe` 中的 `a0` 字段。

## Lab 4 整体测试

完成所有练习后，执行以下命令对 Lab 4 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-traps
```

![2026-07-10 19.36.21](./2026-07-10%2019.36.21.png)