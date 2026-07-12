# Lab 5：xv6 的 Copy-on-Write Fork

[English](./lab5-xv6-copy-on-write.md) · [中文](./lab5-xv6-copy-on-write-zh.md)

虚拟内存提供了一层间接性（indirection）：内核可以把 PTE 标记为无效或只读，从而拦截内存访问并触发页错误；也可以通过修改 PTE 来改变地址含义。系统领域常说，很多系统问题都可以通过增加一层间接来解决。本实验将通过一个典型例子来说明：copy-on-write fork（写时复制 fork）。

开始实验前，请切换到 `cow` 分支：

```bash
$ git fetch
$ git checkout cow
$ make clean
```

---

## 问题背景

xv6 中的 `fork()` 会把父进程全部用户态内存复制到子进程。如果父进程很大，这个复制开销会很高。更糟的是，这项工作常常是浪费的：子进程通常在 `fork()` 后很快调用 `exec()`，直接丢弃这份复制出来的内存，往往根本没用到大部分页面。

当然，如果父子进程后续都实际使用某个共享页面，并且其中一方发生写入，那么复制仍然是必要的。

---

## 解决思路

实现 COW `fork()` 的目标是：把物理页面的分配与拷贝推迟到“真正需要时”（如果永远不需要就不做）。

COW `fork()` 只为子进程创建页表，让子进程用户内存 PTE 直接指向父进程当前物理页；并把父子两边对应用户页都标记为只读。之后任一进程尝试写入这些页面时，CPU 会触发页错误。内核页错误处理程序识别到这是 COW 场景后：

1. 为发生写入的进程分配新物理页；
2. 将原页内容拷贝到新页；
3. 把该进程对应 PTE 改为指向新页，并恢复可写权限。

处理返回后，进程即可写入自己的私有副本。

COW `fork()` 同时让“何时释放物理页”变得更复杂：同一个物理页可能被多个进程页表引用，只有最后一个引用消失时才能释放。xv6 这种教学内核里实现还算直接，但生产内核里很容易出错，可参考 [Patching until the COWs come home](https://lwn.net/Articles/849638/)。

---

## 实现写时复制（implement copy-on-write fork）

你的任务是在 xv6 内核中实现 COW `fork`。当修改后的内核能通过 `cowtest` 和 `usertests -q` 时即完成。

xv6 已提供测试程序 `cowtest`（源码在 `user/cowtest.c`）。未修改内核时，最开始就会失败：

```bash
$ cowtest
simple: fork() failed
$
```

`simple` 测试会先申请超过一半可用物理内存，再调用 `fork()`；由于空闲物理内存不足以完整复制父进程，`fork` 会失败。

完成后，期望输出如下（`cowtest` 与 `usertests -q` 都通过）：

```bash
$ cowtest
simple: ok
simple: ok
three: ok
three: ok
three: ok
file: ok
forkfork: ok
ALL COW TESTS PASSED
$ usertests -q
...
ALL TESTS PASSED
$
```

一个可行的实现步骤：

1. 修改 `uvmcopy()`：不再给子进程分配新页，而是把父进程物理页映射给子进程；对原本带 `PTE_W` 的页面，在父子双方 PTE 中清除 `PTE_W`。
2. 修改 `vmfault()`：识别 COW 写页错误。若写错误发生在“原本可写”的 COW 页上，则 `kalloc()` 分配新页、复制旧页内容、更新故障进程 PTE 并恢复 `PTE_W`。对于原本只读（例如代码段）页面，仍保持只读共享；若进程尝试写入，应直接 kill。
3. 保证物理页在“最后一个 PTE 引用消失”时才释放。常用方式是为每个物理页维护引用计数（被用户页表引用的次数）：`kalloc()` 时设为 1；`fork` 共享时递增；任一进程解除映射时递减。`kfree()` 仅在计数为 0 时才把页面放回空闲链表。计数可放在固定长度整型数组中；你需要设计索引和数组大小（例如：用物理地址除以 4096 作为索引，数组长度可取 `kalloc.c` 中 `kinit()` 放入空闲链表的最高物理页范围）。可按需修改 `kalloc.c`（如 `kalloc()` / `kfree()`）实现计数维护。
4. 修改 `copyout()`：遇到 COW 页时，复用与页错误路径一致的处理逻辑。

提示：

- 你可能需要在 PTE 中记录“是否 COW 映射”。可使用 RISC-V PTE 里的 `RSW`（reserved for software）位。
- `usertests -q` 覆盖了一些 `cowtest` 不包含的场景，两个都要跑。
- 页表标志相关宏定义在 `kernel/riscv.h` 文件末尾。
- COW 页错误发生时若无可用内存，应杀死进程。

---

## 提交实验

### 记录耗时

创建 `time.txt`，写入一个整数（本实验耗时小时数），然后 `git add` + `git commit`。

### 回答问题

若本实验包含问答题，请把答案写入 `answers-*.txt`，并 `git add` + `git commit`。

### 提交方式

作业通过 Gradescope 提交。你需要 MIT 的 Gradescope 账号；课程加入码见 Piazza。需要帮助可参考[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

准备提交时，运行 `make zipball` 生成 `lab.zip`，上传至对应作业。

若运行 `make zipball` 时存在未提交或未跟踪文件，可能看到：

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

请检查输出，确认解题所需文件都已被跟踪（即不在 `??` 行中）。新文件可通过 `git add {filename}` 纳入版本控制。

- 提交前运行 `make grade`，确保测试通过。Gradescope 自动评分使用同一评分程序。
- 运行 `make zipball` 前，先提交已修改源码。
- 你可以在 Gradescope 查看提交状态并下载提交内容；Gradescope 分数即该实验最终成绩。

---

## 可选挑战练习

- 量化你的 COW 实现减少了多少 xv6 的拷贝字节数与物理页分配数量，并继续挖掘可进一步优化这些指标的机会。

---

*如有关于 6.1810 的问题或建议，请邮件联系课程组：61810-staff@lists.csail.mit.edu。*
