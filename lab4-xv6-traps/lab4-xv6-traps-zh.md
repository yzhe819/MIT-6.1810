# Lab 4：陷阱（Traps）

[English](./lab4-xv6-traps.md) · [中文](./lab4-xv6-traps-zh.md)

本实验将探索系统调用如何通过 trap 机制实现。你会先完成一个与栈相关的热身练习，然后实现一个用户态 trap 处理的示例。

开始编码前，请阅读 [xv6 手册](xv6/book-riscv-rev5.pdf)第 4 章，以及以下相关源码：

- `kernel/trampoline.S`：负责用户态与内核态切换的汇编代码
- `kernel/trap.c`：处理所有中断的代码

开始实验前，请切换到 `traps` 分支：

```bash
$ git fetch
$ git checkout traps
$ make clean
```

---

## RISC-V 汇编（easy）

理解一些 RISC-V 汇编知识在本实验中很重要（你在 6.1910 / 6.004 中已经接触过）。在 xv6 仓库中有一个文件 `user/call.c`。执行 `make fs.img` 时会编译它，并在 `user/call.asm` 生成可读的汇编版本。

阅读 `call.asm` 中函数 `g`、`f`、`main` 的代码。RISC-V 指令手册可在[参考页面](reference.html)找到。请把下面问题写入 `answers-traps.txt`：

1. 函数参数放在哪些寄存器中？例如，`main` 调用 `printf` 时参数 `13` 位于哪个寄存器？
2. 在 `main` 的汇编代码中，调用函数 `f` 的位置在哪里？调用 `g` 的位置在哪里？（提示：编译器可能会内联函数。）
3. 函数 `printf` 的地址是多少？
4. 在 `main` 中执行跳转到 `printf` 的 `jalr` 之后，寄存器 `ra` 的值是什么？
5. 对于下面代码，`'y='` 后面会打印什么？（注意：答案不是某个固定值。）为什么会这样？

```c
printf("x=%d y=%d", 3);
```

---

## 调用栈回溯（moderate）

调试时，backtrace（回溯）通常很有用：它列出错误点之上栈中的函数调用链。为了支持回溯，编译器生成的机器码会为当前调用链中的每个函数维护一个栈帧。每个栈帧包含返回地址和指向调用者栈帧的“帧指针”。寄存器 `s0` 保存当前栈帧指针（准确说，它指向“栈上已保存返回地址的位置 + 8”）。你的 backtrace 应该使用帧指针向上遍历栈，并打印每个栈帧中的已保存返回地址。

请在 `kernel/printf.c` 中实现 `backtrace()`，并在 `sys_pause` 中调用它。然后运行 `bttest`（会调用 `sys_pause`）。输出应类似（具体地址可能不同）：

```
backtrace:
0x0000000080002cda
0x0000000080002bb6
0x0000000080002898
```

`bttest` 结束后退出 QEMU。在终端中运行 `addr2line -e kernel/kernel`（或 `riscv64-unknown-elf-addr2line -e kernel/kernel`），并粘贴 backtrace 中的地址：

```bash
$ addr2line -e kernel/kernel
0x0000000080002de2
0x0000000080002f4a
0x0000000080002bfc
Ctrl-D
```

你应看到类似：

```
kernel/sysproc.c:74
kernel/syscall.c:224
kernel/trap.c:85
```

提示：

- 在 `kernel/defs.h` 中声明 `backtrace()` 原型，以便在 `sys_pause` 中调用。
- GCC 会把当前执行函数的帧指针放在 `s0` 寄存器中。在 `kernel/riscv.h` 的 `#ifndef __ASSEMBLER__ ... #endif` 区域加入：

```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x));
  return x;
}
```

然后在 `backtrace` 中调用它读取当前帧指针。`r_fp()` 通过[in-line assembly](https://gcc.gnu.org/onlinedocs/gcc/Using-Assembly-Language-with-C.html)读取 `s0`。
- 这些[课程讲义](https://pdos.csail.mit.edu/6.1810/2023/lec/l-riscv.txt)有栈帧布局图。注意：返回地址位于帧指针固定偏移 `-8`；保存的帧指针位于固定偏移 `-16`。
- `backtrace()` 需要判断何时到达最后一个栈帧并停止。一个有用事实是：每个内核栈分配为一个按页对齐的单页，因此同一栈的所有帧都在同一页内。可使用 `PGROUNDDOWN(fp)`（见 `kernel/riscv.h`）识别帧指针所在页面。

当 backtrace 可用后，在 `kernel/printf.c` 的 `panic` 中调用它，以便内核 panic 时打印回溯。

---

## 周期性通知（hard）

本练习要求你为 xv6 增加一个功能：进程在消耗 CPU 时间时，内核可周期性通知该进程。这对计算密集型程序很有用（例如限制 CPU 占用，或在计算过程中定期执行动作）。更一般地说，你将实现一种“用户态中断/故障处理器”的基础机制；类似思路也可用于应用层页错误处理。若通过 `alarmtest` 和 `usertests -q`，则此部分正确。

你需要新增系统调用 `sigalarm(interval, handler)`。若应用调用 `sigalarm(n, fn)`，则每消耗 `n` 个 CPU tick，内核应触发执行应用函数 `fn`。`fn` 返回后，应用应从中断处继续执行。tick 是 xv6 中由硬件定时器中断频率决定的时间单位。若应用调用 `sigalarm(0, 0)`，内核应停止周期性 alarm 调用。

在 xv6 仓库里有 `user/alarmtest.c`，把它加入 Makefile。在你实现 `sigalarm` 和 `sigreturn` 系统调用之前（见下文），它不会正确编译。

`alarmtest` 在 `test0` 中调用 `sigalarm(2, periodic)`，要求内核每 2 个 tick 调用一次 `periodic()`，随后进行一段忙等待。你可以查看 `user/alarmtest.asm` 辅助调试。当 `alarmtest` 输出类似下列内容且 `usertests -q` 也通过时，即为正确：

```bash
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
test3 start
test3 passed
$ usertests -q
...
ALL TESTS PASSED
$
```

最终实现代码行数可能不多，但细节较难。评分会使用仓库中的原版 `alarmtest.c`。你可以临时修改 `alarmtest.c` 便于调试，但务必确保原版也能全部通过。

### test0：触发 handler

第一步，修改内核使其跳转到用户态 alarm handler，从而让 `test0` 打印 `alarm!`。此阶段可先不处理 `alarm!` 之后行为；即使打印后崩溃也可以接受。

提示：

- 修改 Makefile，使 `alarmtest.c` 被编译为 xv6 用户程序。
- 在 `user/user.h` 中加入声明：

```c
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```

- 更新 `user/usys.pl`（会生成 `user/usys.S`）、`kernel/syscall.h`、`kernel/syscall.c`，使 `alarmtest` 能调用 `sigalarm` 与 `sigreturn`。
- 目前可让 `sys_sigreturn` 直接返回 0。
- `sys_sigalarm()` 应将报警间隔和处理函数指针存入 `proc` 结构（`kernel/proc.h`）新增字段。
- 还需在 `struct proc` 中增加字段，记录自上次 handler 调用以来经过的 tick（或距下次调用剩余 tick）。可在 `proc.c` 的 `allocproc()` 初始化。
- 每个 tick 都会触发硬件时钟中断，该中断在 `kernel/trap.c` 的 `usertrap()` 中处理。
- 只在定时器中断时更新 alarm tick，类似：

```c
if (which_dev == 2) ...
```

- 仅当进程有激活的定时器时才调用 alarm 函数。注意用户 alarm 函数地址可能是 0（例如 `user/alarmtest.asm` 中 `periodic` 地址就是 0）。
- 修改 `usertrap()`，当 alarm 间隔到期时，让用户进程执行 handler。思考：RISC-V trap 返回用户态时，是什么决定用户代码恢复执行的指令地址？
- 用单 CPU 跑 QEMU 更便于 GDB 调试：

```bash
make CPUS=1 qemu-gdb
```

- 当 `alarmtest` 能打印 `alarm!` 时，说明这一步成功。

### test1/test2()/test3()：恢复被中断代码

你很可能会遇到：`alarmtest` 在打印 `alarm!` 后于 `test0` 或 `test1` 崩溃，或最终打印 `test1 failed`，或未打印 `test1 passed` 就退出。要修复这个问题，必须确保 handler 完成后，控制流返回到“被定时器中断打断的那条用户指令”。还必须恢复中断时寄存器内容，保证用户程序在 alarm 之后无扰动继续执行。最后，每次 alarm 触发后需重新装填计数器，使 handler 周期性执行。

作为起点，实验已为你做了一个设计决定：用户 alarm handler 在完成后必须调用 `sigreturn` 系统调用。可参考 `alarmtest.c` 中 `periodic`。这意味着你可以让 `usertrap` 与 `sys_sigreturn` 协同工作，使用户进程在处理完 alarm 后正确恢复。

提示：

- 你需要保存并恢复寄存器。为了正确恢复被中断代码，需要保存哪些寄存器？（提示：很多。）
- 在定时器触发时，让 `usertrap` 在 `struct proc` 中保存足够状态，以便 `sigreturn` 能正确返回到被中断用户代码。
- 防止 handler 重入：若上一个 handler 尚未返回，内核不应再次调用它。`test2` 会测试这一点。
- 确保恢复 `a0`。`sigreturn` 本身是系统调用，其返回值放在 `a0` 中。

通过 `test0`、`test1`、`test2`、`test3` 后，再运行 `usertests -q` 确认没有破坏内核其他功能。

---

## 提交实验

### 记录耗时

创建 `time.txt`，写入一个整数表示本实验耗时（小时）。然后执行 `git add` 和 `git commit`。

### 回答问题

若本实验包含问答题，请将答案写入 `answers-*.txt`，并执行 `git add` 和 `git commit`。

### 提交方式

作业通过 Gradescope 提交。你需要 MIT 的 Gradescope 账号。课程入口码见 Piazza。若需要可参考[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

准备提交时，运行 `make zipball` 生成 `lab.zip`，并上传到对应的 Gradescope 作业。

若运行 `make zipball` 时存在未提交修改或未跟踪文件，可能出现如下输出：

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

请检查这些行，确保解题所需文件都已被跟踪（即不在以 `??` 开头的行中）。你可通过 `git add {filename}` 将新文件加入版本控制。

- 提交前请运行 `make grade`，确保通过全部测试。Gradescope 自动评分器使用同一评分程序。
- 运行 `make zipball` 前请先提交所有修改过的源码。
- 你可以在 Gradescope 查看提交状态并下载提交内容。Gradescope 上的实验分数就是最终实验成绩。

---

## 可选挑战练习

- 在 `backtrace()` 中打印函数名和行号，而不是纯数值地址。

---

*如有关于 6.1810 的问题或意见，请发邮件给课程组：61810-staff@lists.csail.mit.edu。*
