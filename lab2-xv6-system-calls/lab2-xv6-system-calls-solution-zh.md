## Lab 2 实验报告

**目标简述：** 理解系统调用（System Call）机制，以及用户态（User Mode）与内核态（Kernel Mode）之间的切换过程

**实验难度：** `Using GDB` (Easy)

### Using GDB (Easy)

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

#### Q1: 查看调用栈输出，哪个函数调用了 syscall？

观察 `backtrace` 的输出可以发现，位于 `kernel/trap.c` 第 68 行的 `usertrap()` 函数调用了 `syscall()`。查阅对应源码可以确认，这正是用户态 trap 处理流程中负责分发系统调用的入口。

---

随后使用 `n` 命令单步执行，逐行推进。执行完 `struct proc *p = myproc();` 之后，在 GDB 界面输入 `p /x *p`，以十六进制格式打印当前进程的 `proc` 结构体（定义见 `kernel/proc.h`）。

![2026-06-25 21.44.34](./2026-06-25%2021.44.34.png)

![2026-06-25 21.45.31](./2026-06-25%2021.45.31.png)

---

#### Q2: p->trapframe->a7 的值是多少？它代表什么含义？（提示：参考 user/init.c 和 user/init.asm）

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

#### Q3: trap 发生之前，CPU 处于哪种特权模式？

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

#### Q4: 写出内核发生 panic 时对应的汇编指令。变量 num 对应哪个寄存器？

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

#### Q5: 内核为何崩溃？提示：参考教材图 3-3，地址 0 是否在内核地址空间中有映射？scause 的值是否印证了这一点？

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

#### Q6: 内核发生 panic 时，正在运行的是哪个进程？其进程 ID（pid）是多少？

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
