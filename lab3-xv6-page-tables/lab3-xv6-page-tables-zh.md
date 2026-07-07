# Lab 3：页表

[English](./lab3-xv6-page-tables.md) · [中文](./lab3-xv6-page-tables-zh.md)

本实验将带你探索页表，并通过修改页表实现一些常见的操作系统功能。

在开始编码之前，请先阅读 [xv6 手册](xv6/book-riscv-rev5.pdf)第 3 章，以及以下相关文件：

- `kernel/memlayout.h`：描述内存布局。
- `kernel/vm.c`：包含大部分虚拟内存（VM）代码。
- `kernel/kalloc.c`：包含物理内存分配与释放代码。

你也可以参考 [RISC-V 特权架构手册](https://drive.google.com/file/d/17GeetSnT5wW3xNuAHI95-SI1gPGd5sJ_/view?usp=drive_link)。

开始实验前，请切换到 `pgtbl` 分支：

```bash
$ git fetch
$ git checkout pgtbl
$ make clean
```

---

## 检查用户进程页表

为帮助你理解 RISC-V 页表，第一项任务是解释一个用户进程的页表内容。

运行 `make qemu` 后执行用户程序 `pgtbltest`。`print_pgtbl` 函数会通过本实验新增的 `pgpte` 系统调用，打印 `pgtbltest` 进程前 10 页和后 10 页的页表项。输出大致如下：

```
va 0 pte 0x21FCF45B pa 0x87F3D000 perm 0x5B
va 1000 pte 0x21FCE85B pa 0x87F3A000 perm 0x5B
...
va 0xFFFFD000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFFE000 pte 0x21FD80C7 pa 0x87F60000 perm 0xC7
va 0xFFFFF000 pte 0x20001C4B pa 0x80007000 perm 0x4B
```

请针对 `print_pgtbl` 输出中的每一个页表项，解释其逻辑含义及权限位。xv6 手册中的图 3.4 可能会有帮助，但注意图中的页集合可能与当前被检查进程略有差异。另外，xv6 并不会将虚拟页连续地放在物理内存中。

---

## 加速系统调用

一些操作系统（例如 Linux）会通过在用户态和内核态之间共享一个只读区域来加速某些系统调用。这样可以避免执行这些系统调用时的内核态切换。为了帮助你学习如何向页表插入映射，本任务要求你在 xv6 中为 `getpid()` 实现该优化。

在每个进程创建时，在 `USYSCALL`（定义于 `memlayout.h` 的虚拟地址）处映射一个只读页。在该页起始位置放置 `struct usyscall`（同样定义于 `memlayout.h`），并初始化为当前进程的 PID。本实验的用户态已提供 `ugetpid()`，它会自动使用 `USYSCALL` 映射。运行 `pgtbltest` 时若 `ugetpid` 测试通过，则本部分可获得满分。

提示：

- 选择适当的权限位，使用户态只能读取该页。
- 一个新页在生命周期中涉及若干步骤，可参考 `kernel/proc.c` 中 trapframe 的处理流程。

思考：还有哪些 xv6 系统调用可以通过这个共享页进一步加速？请说明实现思路。

---

## 打印页表

为了帮助你可视化 RISC-V 页表并便于后续调试，你的下一项任务是编写一个函数，打印页表内容。

我们新增了系统调用 `kpgtbl()`，它会调用 `vm.c` 中的 `vmprint()`。该调用接收一个 `pagetable_t` 参数，你的任务是按如下格式打印该页表。

运行 `print_kpgtbl()` 测试时，你的实现应打印类似下面的输出：

```
page table 0x0000000087f22000
 ..0x0000000000000000: pte 0x0000000021fc7801 pa 0x0000000087f1e000
 .. ..0x0000000000000000: pte 0x0000000021fc7401 pa 0x0000000087f1d000
 .. .. ..0x0000000000000000: pte 0x0000000021fc7c5b pa 0x0000000087f1f000
 .. .. ..0x0000000000001000: pte 0x0000000021fc705b pa 0x0000000087f1c000
 .. .. ..0x0000000000002000: pte 0x0000000021fc6cd7 pa 0x0000000087f1b000
 .. .. ..0x0000000000003000: pte 0x0000000021fc6807 pa 0x0000000087f1a000
 .. .. ..0x0000000000004000: pte 0x0000000021fc64d7 pa 0x0000000087f19000
 ..0x0000003fc0000000: pte 0x0000000021fc8401 pa 0x0000000087f21000
 .. ..0x0000003fffe00000: pte 0x0000000021fc8001 pa 0x0000000087f20000
 .. .. ..0x0000003fffffd000: pte 0x0000000021fd4813 pa 0x0000000087f52000
 .. .. ..0x0000003fffffe000: pte 0x0000000021fd00c7 pa 0x0000000087f40000
 .. .. ..0x0000003ffffff000: pte 0x0000000020001c4b pa 0x0000000080007000
```

第一行显示传给 `vmprint` 的参数。之后每行对应一个 PTE（包括指向更深层页表页的 PTE）。每行前缀的 `" .."` 数量表示其在页表树中的深度。每行应展示虚拟地址、PTE 位值，以及从 PTE 提取出的物理地址。不要打印无效 PTE。以上示例中，顶层页表在索引 0 和 255 处有映射；索引 0 的下一层仅索引 0 有映射；再下一层该索引下有若干映射。

你的代码打印出的物理地址可能与示例不同，这是正常的；但条目数量和虚拟地址应一致。

提示：

- 使用 `kernel/riscv.h` 文件末尾提供的宏。
- `freewalk` 函数可作为参考。
- 在 `printf` 中使用 `%p` 打印完整 64 位十六进制 PTE 和地址（如示例所示）。

请针对 `vmprint` 输出中的每个叶子页，解释其逻辑含义、权限位，以及它与前面 `print_pgtbl()` 输出之间的对应关系。xv6 手册图 3.4 可能有帮助，但其页集合可能与当前进程略有差异。

---

## 使用 Superpage（大页）

RISC-V 分页硬件支持 2MB 页面以及常规 4096 字节页面。更大页面的一般概念称为 superpage（大页）；由于 RISC-V 支持多种页大小，2MB 页面通常也称为 megapage。操作系统通过在一级 PTE 中设置 `PTE_V` 和 `PTE_R` 位，并将物理页号指向某个 2MB 物理内存区域的起始地址来创建 superpage。该物理地址必须按 2MB 对齐（即 2MB 的整数倍）。你可以在 RISC-V 特权手册中搜索 megapage/superpage（尤其是第 112 页顶部）了解更多。使用 superpage 可以减少页表占用的物理内存，并降低 TLB 未命中；对某些程序会显著提升性能。

你的任务是修改 xv6 内核以使用 superpage。具体来说：如果某个用户程序调用 `sbrk()` 申请 2MB 或更大的空间，且新增长的地址范围中包含一个或多个“按 2MB 对齐且至少 2MB 大小”的区域，内核应使用单个 superpage（而不是数百个普通页）进行映射。运行 `pgtbltest` 时若 `superpg_fork` 与 `superpg_free` 测试通过，则本部分可获得满分。

提示：

- 阅读 `user/pgtbltest.c` 中的 `superpg_fork` 与 `superpg_free` 测试。
- 可从 `kernel/sysproc.c` 中的 `sys_sbrk` 开始（它由 `sbrk` 系统调用触发），沿调用路径追踪到 `growproc`，该函数会为 `sbrk` 立即分配内存。
- 内核需要具备分配/释放 2MB 内存区域的能力。修改 `kalloc.c`，预留少量 2MB 物理内存区域，并实现 `superalloc()` 与 `superfree()`；只需少量 2MB 块即可。
- 含 superpage 的进程在 `fork` 时必须分配对应 superpage，在 `exit` 时必须释放；因此你需要修改 `uvmcopy()` 与 `uvmunmap()`。
- 当 `sbrk` 只部分释放一个 superpage（例如释放 superpage 最后 4096 字节）时，需要把 superpage“降级”为普通页。

真实操作系统会动态地将一组普通页提升为 superpage。下面的论文解释了这样做的价值，以及在更严肃设计中的困难点：

[Juan Navarro, Sitaram Iyer, Peter Druschel, and Alan Cox. Practical, transparent operating system support for superpages. SIGOPS Oper. Syst. Rev., 36(SI):89-104, December 2002.](https://www.usenix.org/conference/osdi-02/practical-transparent-operating-system-support-superpages)

以下参考总结了不同操作系统中的 superpage 管理机制与策略：

[A comprehensive analysis of superpage management mechanism and policies](https://www.usenix.org/conference/atc20/presentation/zhu-weixi)

---

## 提交实验

### 记录耗时

创建新文件 `time.txt`，写入一个整数，表示你在本实验中花费的小时数。然后执行 `git add` 和 `git commit`。

### 回答问题

如果本实验包含问答题，请将答案写入 `answers-*.txt`，并执行 `git add` 和 `git commit`。

### 提交方式

课程作业通过 Gradescope 提交。你需要 MIT 的 Gradescope 账号。加入课程的入口码请查看 Piazza。如需帮助可参考 [此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

准备提交时，运行 `make zipball` 生成 `lab.zip`，并上传到对应的 Gradescope 作业。

如果运行 `make zipball` 时存在未提交修改或未追踪文件，你可能会看到如下输出：

```
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

请检查上述输出，确保你的实验解答所需文件都已被追踪（即不在以 `??` 开头的行中）。你可以使用 `git add {filename}` 将新文件纳入追踪。

- 提交前请运行 `make grade`，确保代码通过全部测试。Gradescope 自动评分器使用同一套评分程序。
- 运行 `make zipball` 前，请先提交所有修改过的源码。
- 你可以在 Gradescope 查看提交状态并下载已提交代码。Gradescope 上的实验分数即为最终实验成绩。

---

## 可选挑战练习

- 基于上文论文中的思路，改进你的 superpage 设计，使其更贴近真实系统。
- 取消用户进程第一个页面的映射，使空指针解引用触发缺页异常。你需要修改 `user.ld`，例如把用户态代码段起始地址从 `0` 改为 `4096`。
- 添加一个系统调用，用 `PTE_D` 报告脏页（被修改过的页面）。

---

*如有关于 6.1810 的问题或建议，请发送邮件至课程团队：61810-staff@lists.csail.mit.edu。*
