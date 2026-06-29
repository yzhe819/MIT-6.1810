# Lab 1：Xv6 与 Unix 实用工具

[English](./lab1-xv6-unix-utilities.md) · [中文](./lab1-xv6-unix-utilities-zh.md)

本实验将带你熟悉 xv6 及其系统调用。


## 启动 Xv6 `（简单）`

请查阅[实验工具页面](#)，了解如何配置计算机以运行本系列实验。

### 克隆仓库

```bash
$ git clone git://g.csail.mit.edu/xv6-labs-2025
$ cd xv6-labs-2025
```

本实验及后续实验所需的文件均通过 Git 分发。每个实验对应一个为其定制的 xv6 版本，需切换到相应分支获取。如需了解 Git 的使用，可参阅 [Git 用户手册](#)或[这份面向 CS 学生的 Git 概览](#)。

### 使用 Git 保存进度

完成一道练习后，可通过以下命令提交进度：

```bash
$ git commit -am 'my solution for util lab exercise 1'
Created commit 60d2135: my solution for util lab exercise 1
 1 files changed, 1 insertions(+), 0 deletions(-)
```

常用 Git 命令：

- `git diff` — 查看自上次提交以来的修改
- `git diff origin/util` — 查看相对于 util 初始代码的修改（`origin/util` 是本实验所在的 Git 分支名）

### 构建并运行 Xv6

```bash
$ make qemu
riscv64-unknown-elf-gcc    -c -o kernel/entry.o kernel/entry.S
riscv64-unknown-elf-gcc -Wall -Werror -O -fno-omit-frame-pointer -ggdb -DSOL_UTIL -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib -mno-relax -I. -fno-stack-protector -fno-pie -no-pie   -c -o kernel/start.o kernel/start.c
...
riscv64-unknown-elf-ld -z max-page-size=4096 -N -e main -Ttext 0 -o user/_zombie user/zombie.o user/ulib.o user/usys.o user/printf.o user/umalloc.o
riscv64-unknown-elf-objdump -S user/_zombie > user/zombie.asm
riscv64-unknown-elf-objdump -t user/_zombie | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$/d' > user/zombie.sym
mkfs/mkfs fs.img README  user/xargstest.sh user/_cat user/_echo user/_forktest user/_grep user/_init user/_kill user/_ln user/_ls user/_mkdir user/_rm user/_sh user/_stressfs user/_usertests user/_grind user/_wc user/_zombie
nmeta 46 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 1) blocks 954 total 1000
balloc: first 591 blocks have been allocated
balloc: write bitmap block at sector 45
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$
```

### 用 `ls` 验证

```bash
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2227
xargstest.sh   2 3 93
cat            2 4 32864
echo           2 5 31720
forktest       2 6 15856
grep           2 7 36240
init           2 8 32216
kill           2 9 31680
ln             2 10 31504
ls             2 11 34808
mkdir          2 12 31736
rm             2 13 31720
sh             2 14 54168
stressfs       2 15 32608
usertests      2 16 178800
grind          2 17 47528
wc             2 18 33816
zombie         2 19 31080
console        3 20 0
```

这些是 mkfs 写入初始文件系统的文件，其中大多数是可直接运行的程序。你刚才运行的 `ls` 便是其中之一。

> **提示**
> - xv6 没有 `ps` 命令，但可以按 `Ctrl-p` 让内核打印每个运行中进程的信息。此刻尝试的话，你会看到两行输出：一行对应 `init`，一行对应 `sh`。
> - 退出 QEMU：先按 `Ctrl-a`，再按 `x`。


## 练习

### 1. `sleep` `（简单）`

本练习旨在让你熟悉如何在 xv6 上编写用户态程序，以及如何使用暂停系统调用。

请为 xv6 实现一个用户态的 `sleep` 程序，功能类似于 Unix 的 sleep 命令。你的 sleep 程序应暂停指定数量的时钟滴答（tick）。时钟滴答是 xv6 内核定义的时间单位，即定时器芯片两次中断之间的间隔。

**文件：** `user/sleep.c`

**预期行为：**

```bash
$ make qemu
...
init: starting sh
$ sleep 10
（稍等片刻，无任何输出）
$
```

**提示：**

- 开始编码前，请先阅读 xv6 手册的第 1 章。
- 参阅 `user/` 目录下的其他程序（如 `user/echo.c`、`user/grep.c`、`user/rm.c`），了解命令行参数是如何传递给程序的。
- 将你的 sleep 程序添加到 `Makefile` 的 `UPROGS` 列表中；完成后，`make qemu` 将编译该程序，你便能在 xv6 shell 中运行它。
- 若用户忘记传入参数，sleep 应打印错误信息。
- 命令行参数以字符串形式传入；可使用 `atoi`（见 `user/ulib.c`）将其转换为整数。
- 使用系统调用 `pause()`。
- 参阅 `kernel/sysproc.c` 了解实现 `pause()` 系统调用的内核代码（搜索 `sys_pause`）；参阅 `user/user.h` 了解用户程序可调用的 C 声明；参阅 `user/usys.S` 了解从用户态跳转到内核态的汇编代码。
- 如需学习 C 语言，可参阅 Kernighan 与 Ritchie 所著的《C 程序设计语言（第二版）》（K&R）。

**运行测试：**

```bash
$ ./grade-lab-util sleep
# 或
$ make GRADEFLAGS=sleep grade
```


### 2. `sixfive` `（中等）`

本练习将用到系统调用 `open` 和 `read`、C 字符串，以及在 C 中处理文本文件。

对于每个输入文件，`sixfive` 需打印文件中所有 5 或 6 的倍数。数字是由字符串 `" -\r\t\n./"` 中的分隔符隔开的十进制数字序列。因此，`"xv6"` 中的 `6` 不应被打印（因为它不是独立的数字），而 `"/6,"` 中的 `6` 则应被打印。

**预期行为：**

```bash
$ sixfive sixfive.txt
5
100
18
6
```

**提示：**

- 每次读取输入文件的一个字符。
- 可使用 `strchr`（见 `user/ulib.c`）判断某个字符是否属于分隔符集合。
- 文件的开头和结尾隐式视为分隔符。


### 3. `memdump` `（简单）`

本练习将进一步加深你对 C 指针的理解。开始前，请阅读《C 程序设计语言（第二版）》（K&R）5.1 节（指针与地址）至 5.6 节（指针数组），以及 6.4 节（指向结构体的指针）。

请查看 `user/memdump.c`，并实现函数 `memdump(char *fmt, char *data)`。该函数的作用是按照 `fmt` 参数描述的格式，打印 `data` 所指向的内存内容。格式是一个 C 字符串——每个字符指定如何打印数据的连续部分。含多个字段的 C 结构体，可通过包含多个字符的格式字符串来打印。

你可以在 `memdump()` 中直接使用 C 的 `printf()`。

**格式字符说明：**

| 字符 | 含义 |
|
| `i` | 将后续 4 字节作为 32 位整数（十进制）打印 |
| `p` | 将后续 8 字节作为 64 位整数（十六进制）打印 |
| `h` | 将后续 2 字节作为 16 位整数（十进制）打印 |
| `c` | 将后续 1 字节作为 8 位 ASCII 字符打印 |
| `s` | 后续 8 字节为指向 C 字符串的 64 位指针；打印该字符串 |
| `S` | 剩余字节为以空字符结尾的 C 字符串；打印该字符串 |

**不带参数时的预期输出：**

```
$ memdump
Example 1:
61810
2025
Example 2:
a string
Example 3:
another
Example 4:
BD0
1819438967
100
z
xyzzy
Example 5:
hello
w
o
r
l
d
```

> Example 4 输出的第一行（十六进制地址）因运行环境不同而可能有所差异。

**带参数时（从标准输入读取）：**

```bash
$ echo deadc0de | memdump hhcccc
25956
25697
c
0
d
e

$ echo deadc0de | memdump p
64616564
```


### 4. `find` `（中等）`

本练习探索路径名与目录，以及系统调用 `open`、`read` 和 `fstat`。

为 xv6 编写一个简化版的 Unix `find` 程序：在目录树中查找所有具有特定名称的文件。

**文件：** `user/find.c`

**预期行为：**

```bash
$ make qemu
...
init: starting sh
$ echo > b
$ mkdir a
$ echo > a/b
$ mkdir a/aa
$ echo > a/aa/b
$ find . b
./b
./a/b
./a/aa/b
```

**提示：**

- 参阅 `user/ls.c`，了解如何读取目录。
- 使用递归使 find 能够进入子目录。
- 不要递归进入 `.` 和 `..`。
- 每次执行 `make qemu` 都会构建一个新的 `fs.img`，删除上次运行中创建的文件。若希望沿用上次的文件系统，请运行 `make qemu-fs`。
- 需要用到 C 字符串，可参阅 K&R 第 5.5 节。
- 注意：`==` 不能像 Python 那样比较字符串，应使用 `strcmp()`。
- 将程序添加到 `Makefile` 的 `UPROGS` 列表中。

```bash
$ make grade
```


### 5. `exec` `（中等）`

本练习涉及系统调用 `fork`、`exec` 和 `wait`。

为 `find` 增加 `-exec cmd` 标志：对 `find` 找到的每个文件，执行 `cmd file` 命令，而不是打印匹配的文件名。

**预期行为：**

```bash
$ find . wc -exec echo hi
hi ./wc
```

> 此处命令为 `echo hi`，文件为 `./wc`，完整调用即 `echo hi ./wc`，输出 `hi ./wc`。

**提示：**

- 使用 `fork` 和 `exec` 对每个文件调用命令，在父进程中使用 `wait` 等待子进程执行完毕。
- `kernel/param.h` 中声明了 `MAXARG`，在需要声明 `argv` 数组时可能用到。

**运行测试：**

```bash
$ sh < findtest.sh
```

预期输出：

```
$ make qemu
...
init: starting sh
$ sh < findtest.sh
$ echo DONE
$ $ $ $ $ hello
hello
hello
$ $
```

> 输出中出现大量 `$` 是因为 xv6 的 shell 无法识别自己正在处理来自文件的命令而非控制台输入，因此会为文件中的每条命令都打印一个 `$` 提示符。


## 提交实验

### 1. 记录耗时

创建 `time.txt`，内容为本实验所花费的小时数（整数）：

```bash
$ git add time.txt
$ git commit time.txt
```

### 2. 撰写答案

若本实验包含问题，请将答案写入 `answers-*.txt`，然后执行：

```bash
$ git add answers-*.txt
$ git commit answers-*.txt
```

### 3. 通过 Gradescope 提交

作业提交通过 Gradescope 处理。你需要一个 MIT Gradescope 账号，请在 Piazza 上查找加入课程的邀请码。

```bash
$ make zipball
```

将生成的 `lab.zip` 上传到 Gradescope 对应的作业条目。

> 若运行 `make zipball` 时出现未追踪文件（`??`），请先用 `git add <文件名>` 将其纳入追踪，否则这些文件不会被打包提交。

### 4. 最终检查

- 运行 `make grade`，确保代码通过所有测试。Gradescope 的自动评分程序与此相同。
- 在运行 `make zipball` 前，提交所有已修改的源代码。
- 你可以在 Gradescope 上查看提交状态并下载已提交的代码。Gradescope 上的实验成绩即为你的最终实验成绩。


## 可选挑战练习

- **`uptime`** `（简单）` — 编写一个 `uptime` 程序，使用 `uptime` 系统调用以时钟滴答为单位打印系统运行时间。
- **在 `find` 中支持正则表达式** `（简单）` — 在名称匹配中支持正则表达式。`grep.c` 中已有一些简单的正则表达式支持可供参考。
- **改进 `sh`** — xv6 的 shell（`user/sh.c`）本身也是一个用户程序，它缺少真实 shell 的诸多特性，但你可以对其进行修改和完善。例如：
  - 修改 shell，使其在从文件处理命令时不打印 `$` 提示符 `（中等）`
  - 支持 `wait` `（简单）`
  - 支持 Tab 补全 `（简单）`
  - 保存历史命令记录 `（中等）`
  - 或任何你希望 shell 具备的功能。如果目标较为宏大，可能需要修改内核以支持所需的内核特性；xv6 目前支持的内核功能十分有限。


*有关 6.1810 的问题或意见？请发送邮件至课程团队：61810-staff@lists.csail.mit.edu*
