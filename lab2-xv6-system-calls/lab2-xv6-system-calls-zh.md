# Lab 2：系统调用

[English](./lab2-xv6-system-calls.md) · [中文](./lab2-xv6-system-calls-zh.md)

在上一个实验中，你通过系统调用实现了若干实用工具。本实验将向 xv6 添加新的系统调用，帮助你理解系统调用的工作原理，并深入接触 xv6 内核的部分实现细节。后续实验中还会继续添加更多系统调用。

在开始编写代码之前，请先阅读 [xv6 手册](https://pdos.csail.mit.edu/6.S081/2020/xv6/book-riscv-rev1.pdf) 的第 2 章，以及第 4 章的 4.3 和 4.4 节，并了解以下相关源文件：

- 将系统调用路由到内核的用户态"桩代码"位于 `user/usys.S`，由 `make` 时执行 `user/usys.pl` 生成。声明位于 `user/user.h`。
- 将系统调用分发到对应内核函数的内核态代码位于 `kernel/syscall.c` 和 `kernel/syscall.h`。
- 进程相关代码位于 `kernel/proc.h` 和 `kernel/proc.c`。

切换到 `syscall` 分支以开始本实验：

```bash
$ git fetch
$ git checkout syscall
$ make clean
```

---

## 使用 GDB（easy）

在大多数情况下，打印语句已足够用于调试内核，但有时需要单步执行代码或获取调用栈回溯，此时 GDB 调试器将派上用场。

为熟悉 GDB 的使用，请运行 `make qemu-gdb`，然后在另一个窗口中启动 GDB（参见[指导页面](https://pdos.csail.mit.edu/6.1810/2025/labs/guidance.html)中的 GDB 资料）。打开两个窗口后，在 GDB 窗口中输入：

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

`layout` 命令将窗口分为两部分，显示 GDB 当前所在的源码位置；`backtrace` 打印调用栈回溯。

请将以下问题的回答写入 `answers-syscall.txt`：

1. 观察 `backtrace` 的输出，哪个函数调用了 `syscall`？

2. 多次输入 `n` 跳过 `struct proc *p = myproc();` 语句。执行完该语句后，输入 `p /x *p`，以十六进制打印当前进程的 `proc` 结构体（见 `kernel/proc.h`）。`p->trapframe->a7` 的值是什么，它代表什么含义？（提示：参考 `user/init.c`，即 xv6 启动的第一个用户程序，以及其编译后的汇编文件 `user/init.asm`。）

3. 处理器当前运行在 supervisor 模式，可以打印特权寄存器，例如 `sstatus`（详见 [RISC-V 特权指令文档](https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf)）：
   ```
   (gdb) p /x $sstatus
   ```
   CPU 进入当前模式之前处于哪种模式？

4. xv6 内核包含一致性检查，检查失败时内核会 panic。将 `syscall` 开头的 `num = p->trapframe->a7;` 替换为 `num = * (int *) 0;`，运行 `make qemu`，你将看到类似如下输出：
   ```
   xv6 kernel is booting

   hart 2 starting
   hart 1 starting
   scause=0xd sepc=0x80001bfe stval=0x0
   panic: kerneltrap
   ```
   退出 QEMU。在 `kernel/kernel.asm` 中搜索 `sepc` 对应的地址，写下内核 panic 时正在执行的汇编指令，并指出哪个寄存器对应变量 `num`。

5. 启动 GDB，在出错的 `epc` 地址处设置断点：
   ```
   (gdb) b *0x80001bfe
   Breakpoint 1 at 0x80001bfe: file kernel/syscall.c, line 138.
   (gdb) layout asm
   (gdb) c
   ```
   确认此处的汇编指令与上面找到的一致。内核为什么会崩溃？（提示：参考教材图 3-3，地址 0 是否映射在内核地址空间中？`scause` 的值是否印证了这一点？）

6. 要查看内核 panic 时正在运行的用户进程，可打印进程名称：
   ```
   (gdb) p p->name
   ```
   内核 panic 时正在运行的进程名称是什么？其进程 ID（`pid`）是多少？

如有需要，可回看 [Using the GNU Debugger](https://pdos.csail.mit.edu/6.828/2019/lec/gdb_slides.pdf)。[指导页面](https://pdos.csail.mit.edu/6.1810/2025/labs/guidance.html)也提供了调试建议。

---

## 沙箱化命令（moderate）

本实验将实现对进程的"沙箱化"，限制其可以调用的系统调用。你需要创建一个新的 `interpose` 系统调用，用于指定内核应拒绝调用进程执行哪些系统调用。

`interpose` 接受两个参数：整数 `mask` 和路径 `path`。`mask` 的各个位指定需要拒绝的系统调用。第二个参数将在下一个实验中使用，本实验中始终传入 `"-"`。例如，禁止进程调用 `open` 系统调用：

```c
interpose(1 << SYS_open, "-")
```

该掩码应通过 `fork` 被子进程继承。

课程已提供用户程序 `user/sandbox.c`，它会 fork 一个子进程，在子进程中调用 `interpose()`，然后 exec 目标程序。正确实现后，输出如下：

```
$ sandbox 32768 - cat README
cat: cannot open README
$
```

通过以下测试即视为完成：

```
$ ./grade-lab-syscall sandbox_mask
== Test sandbox_mask == sandbox_mask: OK (1.5s)
```

### 实现提示

- 在 `Makefile` 的 `UPROGS` 中添加 `$U/_sandbox`。
- 在 `user/user.h` 中添加 `interpose` 的函数原型，在 `user/usys.pl` 中添加桩代码，在 `kernel/syscall.h` 中添加系统调用编号。
- 在 `kernel/sysproc.c` 中添加 `sys_interpose()` 函数，将 mask 记录到 `proc` 结构体（见 `kernel/proc.h`）的新字段中。将 `sys_interpose` 加入 `kernel/syscall.c` 的 `syscalls` 数组。
- 修改 `kernel/proc.c` 中的 `kfork()`，使子进程继承父进程的 mask。
- 修改 `kernel/syscall.c` 中的 `syscall()`，在分发前检查该系统调用是否应被拒绝。

---

## 带路径白名单的沙箱（easy）

在本实验中，你将扩展沙箱功能：对于被屏蔽的 `open` 和 `exec` 系统调用，若其路径参数与允许的路径匹配，则予以放行。`sys_interpose` 的第二个参数即为该白名单路径。

```
$ sandbox 32768 README grep xv6 README
xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
...
$ sandbox 32768 README grep xv6 x
grep: cannot open x
```

通过所有沙箱测试即视为完成：

```
$ make grade
== Test sandbox_mask ==   sandbox_mask: OK (9.5s)
== Test sandbox_fork ==   sandbox_fork: OK (0.3s)
== Test sandbox_path ==   sandbox_path: OK (1.1s)
== Test sandbox_most ==   sandbox_most: OK (0.8s)
== Test sandbox_minus ==  sandbox_minus: OK (1.1s)
```

### 实现提示

- 修改 `sys_interpose()` 以保存白名单路径，使用 `argstr` 获取路径参数，并在 `proc` 结构体中声明大小为 `MAXPATH` 的缓冲区。
- 若 `open` 或 `exec` 被屏蔽，在拒绝之前先检查路径是否与白名单匹配，若匹配则允许执行。

---

## 攻击 xv6（moderate）

xv6 内核将用户程序相互隔离，并将内核与用户程序隔离。然而，本实验故意引入了一个漏洞：`kernel/vm.c` 中 `uvmalloc()` 的 `memset(mem, 0, sz)` 调用在编译时被省略；`kernel/kalloc.c` 中用于向空闲页写入随机数据的两处 `memset` 调用同样被省略。三处省略的净效果是：新分配的内存会保留其之前使用时残留的内容。因此，调用 `sbrk()` 分配内存的程序可能收到含有历史数据的页面。

`user/secret.c` 将一段密文写入内存后退出，其占用的内存随即被释放。你的目标是在 `user/attack.c` 中添加代码，找到并打印之前 `secret.c` 写入的密文。

`attack.c` 必须在不修改 xv6 和 `secret.c` 的前提下正常工作。

```
$ secret xyzzy
$ attack
xyzzy
$
```

> 评分程序会运行 `attack` 两次，任意一次输出密文即视为通过。测试使用的密文仅包含数字、大写字母和小写字母。

---

## 提交实验

### 记录耗时

创建文件 `time.txt`，写入一个整数，表示完成本实验所花费的小时数，然后 `git add` 并 `git commit`。

### 提交答案

将 GDB 相关问题的回答写入 `answers-syscall.txt`，并提交。

### 提交方式

运行 `make zipball` 生成 `lab.zip`，将其上传至 Gradescope 对应的作业页面。

若 `make zipball` 提示存在未提交或未追踪的文件，请先通过 `git add` 将所需文件纳入版本控制，再重新打包。

提交前请运行 `make grade` 确认所有测试通过，Gradescope 的自动评分程序与本地评分脚本相同。

---

## 可选挑战练习

在 xv6 中发现一个允许攻击者破坏进程隔离或使内核崩溃的漏洞，并告知课程团队。（Meltdown 等旁路攻击不在讨论范围之内。）

---

*如有关于 6.1810 的问题或意见，请发送邮件至课程团队：61810-staff@lists.csail.mit.edu*
