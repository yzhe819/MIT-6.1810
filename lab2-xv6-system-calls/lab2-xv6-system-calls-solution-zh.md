# Lab 2 实验报告

[English](./lab2-xv6-system-calls-solution.md) · [中文](./lab2-xv6-system-calls-solution-zh.md)

**目标简述：** 理解系统调用（System Call）机制，以及用户态（User Mode）与内核态（Kernel Mode）之间的切换过程

**实验难度：** `Using GDB` (Easy) · `Sandbox a command` (moderate) · `Sandbox with allowed pathnames` (easy) · `Attack xv6` (moderate)

## Using GDB (Easy)

首先，实验要求同时开启两个终端窗口，一个运行 `make qemu-gdb`，另一个运行 `gdb`，以验证 GDB 安装是否完整。本实验在 macOS 环境下进行，因此部分配置与课程标准环境略有不同，需要额外安装适配 RISC-V 架构的 GDB 版本。

安装命令如下：

```bash
brew install riscv64-elf-gdb
riscv64-elf-gdb --version
riscv64-elf-gdb    # 替代课程标准的 gdb 命令，后续均使用此命令启动 GDB
```

两个窗口均正常启动后，界面如下（图一为 `make qemu-gdb`，图二为启动 `riscv64-elf-gdb`）：

![2026-06-25 23.51.11](./2026-06-25%2023.51.11.png)

![2026-06-25 21.39.14](./2026-06-25%2021.39.14.png)

按照实验指引，在 GDB 界面依次输入以下命令：

```
(gdb) b syscall
Breakpoint 1 at 0x80002142: file kernel/syscall.c, line 243.
(gdb) c
Continuing.
[Switching to Thread 1.2]

Thread 2 hit Breakpoint 1, syscall () at kernel/syscall.c:243
243     {
(gdb) layout src
(gdb) backtrace
```

![2026-06-25 21.40.49](./2026-06-25%2021.40.49.png)

继续输入 `layout` 和 `backtrace` 命令后，界面如下：

![2026-06-25 21.41.15](./2026-06-25%2021.41.15.png)

![2026-06-25 21.41.47](./2026-06-25%2021.41.47.png)

`layout src` 展示了当前执行位置对应的源码，`backtrace` 则打印了完整的函数调用栈。

---

### Q1: 查看调用栈输出，哪个函数调用了 syscall？

观察 `backtrace` 的输出可以发现，位于 `kernel/trap.c` 第 68 行的 `usertrap()` 函数调用了 `syscall()`。查阅对应源码可以确认，这正是用户态 trap 处理流程中负责分发系统调用的入口。

---

随后使用 `n` 命令单步执行，逐行推进。执行完 `struct proc *p = myproc();` 之后，在 GDB 界面输入 `p /x *p`，以十六进制格式打印当前进程的 `proc` 结构体（定义见 `kernel/proc.h`）。

![2026-06-25 21.44.34](./2026-06-25%2021.44.34.png)

![2026-06-25 21.45.31](./2026-06-25%2021.45.31.png)

---

### Q2: p->trapframe->a7 的值是多少？它代表什么含义？（提示：参考 user/init.c 和 user/init.asm）

使用命令 `p /x p->trapframe->a7` 可直接打印 `a7` 寄存器的值，结果为 `0xf`，即十进制的 15。

查阅 `user/init.asm`，这是 xv6 系统启动时运行的第一个用户程序：

```asm
int
main(void)
{
   0:	1101                	addi	sp,sp,-32
   2:	ec06                	sd	ra,24(sp)
   4:	e822                	sd	s0,16(sp)
   6:	e426                	sd	s1,8(sp)
   8:	e04a                	sd	s2,0(sp)
   a:	1000                	addi	s0,sp,32
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
   c:	4589                	li	a1,2
   e:	00001517          	auipc	a0,0x1
  12:	96250513          	addi	a0,a0,-1694 # 970 <malloc+0xf8>
  16:	39e000ef          	jal	3b4 <open>
  1a:	04054563          	bltz	a0,64 <main+0x64>
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
```

根据 RISC-V 的系统调用约定，用户程序发起系统调用时，将系统调用号存入寄存器 `a7`。`init` 进程执行的第一个系统调用是 `open("console", O_RDWR)`，查阅 `kernel/syscall.h` 可知 `#define SYS_open 15`，与观测值完全吻合。

完整调用链路如下：

```
init 进程启动
    └─→ open("console", O_RDWR)
            └─→ a7 = 15 (SYS_open)
            └─→ ecall 触发 trap，进入 kernel/syscall.c 的 syscall()
            └─→ num = p->trapframe->a7  // = 0xf = 15
            └─→ syscalls[15]()          // 即 sys_open()
```

---

随后在 GDB 界面执行命令 `p /x $sstatus`，打印 Supervisor Status Register (sstatus) 的完整内容：

![2026-06-25 23.52.06](./2026-06-25%2023.52.06.png)


输出结果为 `$sstatus = 0x200000022`。

---

### Q3: trap 发生之前，CPU 处于哪种特权模式？

查阅 RISC-V 特权指令手册第 4.1.1 节，原文说明如下：

> "The SPP bit indicates the privilege level at which a hart was executing before entering supervisor mode. When a trap is taken, SPP is set to 0 if the trap originated from user mode, or 1 otherwise."

将 `0x200000022` 展开分析各字段：

```
0x200000022

十六进制逐位展开：
  2    0    0    0    0    0    0    2    2
0010 0000 0000 0000 0000 0000 0000 0010 0010

bit 1  (SIE)  = 1  → 当前 Supervisor 模式中断使能
bit 5  (SPIE) = 1  → trap 发生前，中断处于开启状态
bit 8  (SPP)  = 0  → trap 发生前的特权级
bit 33 (SD)   = 1  → 浮点/向量状态寄存器存在脏状态（次要）
```

其中最关键的是 **SPP（bit 8）= 0**。

SPP（Supervisor Previous Privilege）由硬件在 trap 发生时自动记录，表示陷入 Supervisor 模式之前 CPU 所处的特权级：

- `SPP = 0` → 来自 **U-mode（用户态）**
- `SPP = 1` → 来自 **S-mode（内核态）**

**结论：** trap 发生前 CPU 处于用户态（U-mode）。`init` 进程作为 xv6 启动后的第一个用户程序，在用户态调用 `open()` 触发 `ecall`，随后陷入 Supervisor 模式，与实验预期完全一致。

---

至此，本阶段调试完成。关闭 `make qemu-gdb` 和 GDB 两个窗口，回到源码，将 `kernel/syscall.c` 中的：

```c
num = p->trapframe->a7;
```

修改为：

```c
num = *(int *)0;
```

修改后的 `syscall()` 函数如下：

```c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = *(int *)0;    // 故意引入的非法内存访问
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

重新运行 `make qemu`，内核如预期发生崩溃，输出报错信息：`scause=0xd sepc=0x80001d08 stval=0x0`。根据实验要求，在 `kernel/kernel.asm` 中搜索 `sepc` 的值 `0x80001d08`，查看对应的已编译汇编指令。

![2026-06-25 22.07.57](./2026-06-25%2022.07.57.png)

---

### Q4: 写出内核发生 panic 时对应的汇编指令。变量 num 对应哪个寄存器？

![2026-06-25 22.11.29](./2026-06-25%2022.11.29.png)

在 `kernel/kernel.asm` 中找到地址 `0x80001d08` 对应的汇编指令如下：

```asm
80001d08:   00002683    lw  a3, 0(zero)  # 0 <_entry-0x80000000>
```

该指令尝试从虚拟地址 `0x0`（即 `zero` 寄存器加偏移 0）读取一个字，并将结果存入寄存器 `a3`。因此，**变量 `num` 对应寄存器 `a3`**。

随后在 GDB 中对该地址设置断点（`b *0x80001d08`），通过 `layout asm` 查看汇编源码：

![2026-06-25 22.17.30](./2026-06-25%2022.17.30.png)

![2026-06-25 22.17.43](./2026-06-25%2022.17.43.png)

继续执行后，程序如预期停留在该断点处，与 `kernel.asm` 中找到的指令完全吻合——即尝试将地址 `0x0` 的内容加载到寄存器 `a3`：

![2026-06-25 22.28.05](./2026-06-25%2022.28.05.png)

---

### Q5: 内核为何崩溃？提示：参考教材图 3-3，地址 0 是否在内核地址空间中有映射？scause 的值是否印证了这一点？

根据教材图 3-3，xv6 内核的地址空间布局如下：

```
0xFFFFFFFF         ┌─────────────────┐
                   │   DEVSPACE      │  （硬件设备映射区）
0x80100000         ├─────────────────┤
                   │   kernel text   │  （内核代码，从 0x80000000 开始）
                   │   kernel data   │
                   │   free memory   │
0x80000000         ├─────────────────┤  ← 内核地址空间起始
                   │                 │
                   │   (unmapped)    │  ← 此区域无任何页表映射
                   │                 │
0x00000000         └─────────────────┘  ← 地址 0x0 位于此处
```

地址 `0x0` 位于内核页表的未映射区域。当 CPU 执行 `lw a3, 0(zero)` 时，MMU 查询页表后发现该虚拟地址无有效映射，随即触发异常。

查阅 RISC-V 特权手册，`scause = 0xd`（十进制 13）对应 **Load page fault**，其定义正是：试图从一个虚拟地址读取数据，但页表中不存在对应的有效映射。

三个寄存器的值相互印证：

| 寄存器 | 值 | 含义 |
|--------|-----|------|
| `scause` | `0xd` | Load page fault（读内存时页表无映射） |
| `sepc` | `0x80001d08` | 触发异常的指令地址 |
| `stval` | `0x0` | 触发异常的非法访问地址 |

由于内核自身触发的 page fault 属于不可恢复错误（不同于用户进程可被终止后继续运行），内核只能执行 `panic` 并停机。

---

### Q6: 内核发生 panic 时，正在运行的是哪个进程？其进程 ID（pid）是多少？

在 GDB 中执行以下命令，直接打印当前进程信息：

```gdb
(gdb) p p->name
(gdb) p p->pid
```

结果如下：

- **进程名**：`init`
- **进程 ID（pid）**：`1`

![2026-06-25 22.28.27](./2026-06-25%2022.28.27.png)

`init` 是 xv6 启动后创建的第一个用户进程，pid 固定为 1。panic 发生在该进程执行第一个系统调用（`open("console", O_RDWR)`）的过程中，与前述分析完全一致。

## Sandbox a command (moderate)

根据lab提示，首先在 `Makefile` 的 `UPROGS` 中添加 `$U/_sandbox`，在 `user/user.h` 中添加函数原型 `int interpose(int, char*);`，并在 `user/usys.pl` 中注册。这是用户态所需的全部改动。

> The Makefile invokes the perl script user/usys.pl, which produces user/usys.S, the actual system call stubs, which use the RISC-V ecall instruction to transition to the kernel. 

内核侧首先在 `kernel/syscall.h` 中分配新的 system call 编号 `#define SYS_interpose 22`，然后在 `kernel/syscall.c` 中添加函数声明并在 syscalls 数组中把system call number和实际的函数对应上。

`sys_interpose()` 的实现在 `kernel/sysproc.c` 中。

首先需要从用户的命令行输入里面拿到mask argument，翻找一下整个`kernel/sysproc.c`，发现了lab1的老朋友, pause方法在内核里面的实现。

```c
uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
```

参考同文件 `sys_pause()` 的写法，使用 `argint(0, &mask)` 读取第一个整数参数。实现就可以这样写了,并且把mask传参打印出来，检查确保它能拿到。
```c
uint64
sys_interpose(void)
{
  // ref to sys_pause, read the argument from user model
  int n;
  argint(0, &n);
  printf("mask: %d\n", n);
  return 0;
} No newline at end of file
```

> recording the mask argument in a new field in the proc structure (see kernel/proc.h).

根据提示，直接去proc结构体里面加上一个`int mask;`,然后开始修改内核里面的fork实现。

首先我们需要把用户的传参写到pro里面来持久化保存状态，通过myproc()来拿现在的proc，然后直接写入就好

```c
uint64
sys_interpose(void)
{
  // ref to sys_pause, read the argument from user model
  int mask;
  argint(0, &mask);
  printf("mask: %d\n", mask);
  myproc()->mask = mask;
  return 0;
}
```

为使子进程继承父进程的限制，在 `kernel/proc.c` 的 `kfork()` 中添加一行 `np->mask = p->mask`，将 mask 复制给子进程。

```c
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // also copy the mask argument from the parent process
  np->mask = p->mask;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  ···

```

最后在 `kernel/syscall.c` 的 `syscall()` 函数中实现拦截。如果当前proc需要访问的系统权限已经被mask设置拒绝，那就直接拒绝调用。

由于每次进入 `syscall()` 只处理一个 syscall 编号，只需用位运算检查对应的 bit 是否被置位，是则直接返回 `-1`：

```c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    if ((p->mask >> num) & 1) {
      p->trapframe->a0 = -1;
      return;
    }
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

---

## Sandbox with allowed pathnames (easy)

> `interpose` 的第二个参数是允许访问的路径。如果 `open` 或 `exec` 被 mask 屏蔽，但访问的路径与允许路径匹配，则放行该调用。

这个任务读起来比较绕口，意思是添加第二个传参（允许访问的路径），如果路径名在允许访问的路径里，即使已经屏蔽了open或者exec这两个方法，操作系统需要依然让它进行这两种操作。

在 `proc` 结构体中新增 `char path[MAXPATH]` 字段，在 `sys_interpose()` 中用 `argstr(1, path, MAXPATH)` 读取第二个参数，并以 `safestrcpy()` 写入 proc：

```c
uint64
sys_interpose(void)
{
  // ref to sys_pause, read the argument from user model
  int mask;
  char path[MAXPATH];

  argint(0, &mask);
  argstr(1, path, MAXPATH);

  // printf("mask: %d\n", mask);
  // printf("path: %s\n", path);

  myproc()->mask = mask;
  safestrcpy(myproc()->path, path, sizeof(path));

  return 0;
}
```

fork 时同样需要继承 path，在 `kfork()` 中补上 `safestrcpy(np->path, p->path, sizeof(p->path))`。

`open` 和 `exec` 被 mask 时，不能在 `syscall()` 层直接拒绝，因为此时还不知道路径参数是什么。需要让它们进入各自的处理函数后再做判断，因此在 `syscall()` 的拦截逻辑中为这两个 syscall 加上例外：

```c
if ((p->mask >> num) & 1) {
  if (num != SYS_open && num != SYS_exec) {
    p->trapframe->a0 = -1;
    return;
  }
}
```

路径检查分别在 `kernel/sysfile.c` 的 `sys_open()` 和 `sys_exec()` 中完成。以 `sys_open()` 为例，在 `argstr()` 读取路径之后、`begin_op()` 之前插入：

```c
struct proc *p = myproc();
if ((p->mask >> SYS_open) & 1) {
  if (strncmp(path, p->path, MAXPATH) != 0) {
    return -1;
  }
}
```

`sys_exec()` 中同理，将 `SYS_open` 替换为 `SYS_exec` 即可。

```c
struct proc *p = myproc();
if ((p->mask >> SYS_exec) & 1) {
  if (strncmp(path, p->path, MAXPATH) != 0) {
    return -1;
  }
}
```

这个逻辑有点绕，做的时候需要想清楚再动手写。

## Attack xv6 (moderate)

lab问题里面描述，清除新分配页面的逻辑被去掉了，然后将垃圾数据放进空闲页面的逻辑也被省略了。
这就导致了新分配的内存里面会残留老的数据，如果有敏感数据，那就能直接被恶意程序读到了。

需要实现`user/attack.c`方法，把之前写入的敏感数据读出来。

这里用了`strcpy(data, "This may help.");`来定位，感觉有点像网页爬虫里面用html .id和css classname来定位数据。

因为"This may help." 是固定的标记字符串，敏感数据紧随其后偏移 16 字节。扫描申请到的内存，找到该标记后直接打印偏移 16 字节处的内容

```c
#include "kernel/types.h"
#include "user/user.h"

#define DATASIZE (8 * 4096)

int 
main(void) {
  char *mem = sbrk(DATASIZE);
  const char *hint = "This may help.";

  for (int i = 0; i < DATASIZE - 16; i++) {
    if (!strcmp(mem + i, hint)) {
      printf("%s\n", mem + i + 16);
      break;
    }
  }

  exit(0);
}
```

测试生成的秘密字符串保证只包含数字以及大小写字母。可以用手动判断是否完结。

或者找到后可以直接print，因为%s会自动帮你把字符串截断。

## Lab 2 整体测试

完成所有练习后，执行以下命令对 Lab 2 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-syscall
```

![2026-06-29 21.12.39](./2026-06-29%2021.12.39.png)