# Lab 7：锁（Locks）

[English](./lab7-xv6-locks.md) · [中文](./lab7-xv6-locks-zh.md)

本实验将让你实践如何重构代码以提升并行性。在多核机器上，并行性差的常见症状是**锁竞争严重**。要提升并行性，通常需要同时调整数据结构与加锁策略。你将把这些方法应用到 xv6 的内存分配器，以及读写锁实现上。

开始写代码前，请阅读 [xv6 手册](xv6/book-riscv-rev5.pdf) 的以下内容：

- 第 7 章：**Locking（锁）**
- 第 3.5 节：**Code: Physical memory allocator（物理内存分配器代码）**

开始实验：

```bash
$ git fetch
$ git checkout lock
$ make clean
```

---

## 内存分配器（Memory Allocator）

`user/kalloctest` 会对 xv6 分配器施压：3 个进程不断扩张/收缩地址空间，触发大量 `kalloc`/`kfree`。当前实现会在 `kmem.lock` 上出现明显竞争。

`kalloctest` 会打印锁统计，其中重点是：

- `#acquire()`：获取锁次数
- `#test-and-set`：在 `acquire` 自旋循环中失败重试次数（可粗略衡量竞争）

优化前输出大致类似：

```text
$ kalloctest
start test1
...
lock: kmem: #test-and-set 18820 #acquire() 433058
...
tot= 18820
test1 FAIL
...
test4 FAIL m 59382 n 135309
```

具体数字和 top-5 锁顺序会随机器不同而变化。

根因是：当前只有**一个全局空闲链表 + 一把全局锁**。你的任务是改造成更高并行度设计：

1. 使用**每 CPU 一条空闲链表**，每条链表有自己的锁。
2. 当某 CPU 的空闲链表为空时，从其他 CPU 的链表中**偷取（steal）**部分页面。

偷取会引入少量竞争，但远小于单锁单链表设计。

要求：

- 所有相关锁名必须以 `"kmem"` 开头（通过 `initlock` 命名）。
- 运行 `kalloctest`，确认 kmem 竞争显著下降。
- 运行 `usertests sbrkmuch`，确认仍能正确分配/回收全部内存。
- 确保 `usertests -q` 通过。
- `make grade` 应通过 kalloc 相关测试。

优化后参考输出（数字会不同）：

```text
$ kalloctest
...
tot= 0
test1 OK
...
tot= 9046
test4 OK
```

### 提示

- 使用 `kernel/param.h` 中的 `NCPU`。
- 让 `freerange` 把所有空闲内存先放到执行该函数的 CPU 列表里。
- `cpuid()` 只有在中断关闭时才安全；使用 `push_off()`/`pop_off()`。
- 需要格式化锁名时可参考 `kernel/sprintf.c` 的 `snprintf`（也可以把所有锁都命名为 `"kmem"`）。

可选：用竞态检测器运行：

```bash
$ make clean
$ make KCSAN=1 qemu
$ kalloctest
```

在 KCSAN 下 `kalloctest` 可能因性能下降而失败，但不应出现 race 报告。

如果出现 race，可用以下命令把回溯地址还原成函数与行号：

```bash
$ riscv64-linux-gnu-addr2line -e kernel/kernel
```

---

## 读写锁（Read-Write Lock）

这一部分与前半部分相互独立；即使前半没完成，也可单独完成并通过这一部分测试。

在 xv6 中，`sys_pause` 和 `sys_uptime` 读取全局 `ticks` 时会持有 `tickslock`。这样能保证正确性，但也阻止了多个核同时读 `ticks`（其实读读并发是安全的），因此白白损失性能。

常见解决方案是读写锁。读写锁允许：

- **多个读者并发持有**（前提是没有写者）
- **最多一个写者**
- 写者持有期间**不能有读者**

API（见 `kernel/defs.h`）：

```c
void initrwlock(struct rwspinlock*);
void read_acquire(struct rwspinlock*);
void read_release(struct rwspinlock*);
void write_acquire(struct rwspinlock*);
void write_release(struct rwspinlock*);
```

关键语义要求：**写者不能饿死**。也就是说，只要有等待中的写者，后续新到来的读者都必须等待，直到写者成功获取并释放锁。

你需要在 xv6 中实现符合上述 API 与语义的读写自旋锁：

- 补全 `kernel/spinlock.c` 中 API 对应的桩函数。
- 很可能需要修改 `kernel/spinlock.h` 里的 `struct rwspinlock` 定义。

完成后用以下测试验证：

```text
$ rwlktest
rwspinlock_test: step 1
...
rwlktest: passed 3/3
```

并确保 `usertests -q` 仍通过，最终 `make grade` 全部通过。

### 提示

- 先看 `kernel/spinlock.c` 里的 `sys_rwlktest`，可以分阶段实现/验证。
- 若直接运行未改动版本，`rwlktest` 会触发 `panic: acquire`，因为原始自旋锁只允许单线程持有；你需要在 write/read 的 `_acquire_inner` 和 `_release_inner` 中换成你自己的读写锁逻辑。
- 小心并发交错场景（例如读者看到“暂无等待写者”后，写者恰好并发到来）。
- 可以参考 GCC 原子操作内建函数：[gcc __atomic builtins](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html)。

---

## 提交实验

### 记录耗时

创建 `time.txt`，内容为一个整数（本实验耗时小时数），然后 `git add` + `git commit`。

### 回答问题

如果本实验有问答题，请写入 `answers-*.txt` 并提交。

### 提交方式

作业通过 Gradescope 提交。你需要 MIT Gradescope 账号；课程加入码见 Piazza。如需帮助可参考[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

运行 `make zipball` 生成 `lab.zip` 后上传。

若有未提交或未跟踪文件，可能看到：

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

确认解题需要的文件都已被跟踪（不在 `??` 行中），必要时 `git add {filename}`。

- 提交前运行 `make grade`。
- `make zipball` 前先提交源码修改。
- Gradescope 实验分数即最终实验成绩。

---

*如有关于 6.1810 的问题或建议，请发送邮件至课程组：61810-staff@lists.csail.mit.edu。*
