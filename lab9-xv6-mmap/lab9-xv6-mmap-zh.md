# Lab 9：mmap

[English](./lab9-xv6-mmap.md) · [中文](./lab9-xv6-mmap-zh.md)

本实验需要你在 xv6 中实现 `mmap` 与 `munmap`，重点是**文件内存映射（memory-mapped files）**。

`mmap`/`munmap` 是 UNIX 管理地址空间的核心机制，可用于进程间共享内存、把文件映射到地址空间、以及基于缺页异常的用户层机制。

获取实验代码并切换分支：

```bash
$ git fetch
$ git checkout mmap
$ make clean
```

---

## mmap 接口（本实验子集，hard）

```c
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
```

本实验只要求实现以下子集：

- `addr` 固定为 `0`（由内核决定映射虚拟地址）
- 成功返回映射地址，失败返回 `0xffffffffffffffff`
- `len` 为映射字节数（可与文件长度不同）
- `prot` 只会是 `PROT_READ`、`PROT_WRITE` 或二者组合
- `flags` 只会是 `MAP_SHARED` 或 `MAP_PRIVATE`
- `fd` 是待映射文件的已打开描述符
- `offset` 固定为 `0`

### 必须懒分配（lazy）

`mmap()` 必须采用懒处理：

- 在 `mmap()` 中**不要**分配物理页
- 在 `mmap()` 中**不要**读取文件数据
- 在页面第一次访问触发缺页异常时（`usertrap` 或其调用路径）再分配/读入/映射

这样才能保证：映射大文件仍然快速，且可映射超过物理内存大小的文件。

若多个进程映射同一 `MAP_SHARED` 文件，不共享物理页也是允许的（本实验可接受）。

---

## munmap 接口（hard）

```c
int munmap(void *addr, size_t len);
```

`munmap()` 需要移除指定范围内映射。

若页面属于 `MAP_SHARED` 且被修改过，解除映射前要回写文件。

本实验可假设 `munmap` 只会出现以下情况：

- 从映射区起始处解除一段，或
- 从映射区末尾解除一段，或
- 解除整个映射区

无需支持“在映射中间打洞”。

进程退出时，也要把其 `MAP_SHARED` 映射上的修改回写（等价于自动 `munmap`）。

---

## 实现目标（VMA + 缺页处理路径，hard）

实现到以下标准：

- `mmaptest` 全通过
- `usertests -q` 继续全通过

结束时应有类似输出：

```text
$ mmaptest
test basic mmap
test basic mmap: OK
...
test fork: OK
test munmap prevents access: OK
test writes to read-only mapped memory: OK
mmaptest: all tests succeeded
$ usertests -q
...
ALL TESTS PASSED
$
```

---

## 推荐实现步骤与提示

1. 先把 `_mmaptest` 加入 `UPROGS`，补齐 `mmap`/`munmap` 系统调用框架，让 `user/mmaptest.c` 能编译；初始可先返回错误。
2. 为每个进程维护 VMA（虚拟内存区域）信息：
   - 起始地址、长度、权限、flags、关联文件等
   - 内核可用固定大小数组实现（例如 16 项）。
3. 实现 `mmap()`：
   - 在进程地址空间中找空闲区间
   - 建立 VMA 记录
   - VMA 保存 `struct file*`，并增加文件引用计数（如 `filedup`）。
4. 在缺页处理路径中处理“命中 mmap 区域”：
   - 分配物理页
   - 用 `readi` 按正确文件偏移读取 4096 字节到该页
   - 以正确权限映射进用户页表
   - 注意 `readi` 相关 inode 的加锁/解锁。
5. 实现 `munmap()`：
   - 定位对应 VMA 与解除范围
   - 解除页映射（`uvmunmap`）
   - 对 `MAP_SHARED` 页面回写文件
   - 若整个 VMA 被移除，减少对应文件引用计数。
6. 脏页优化：
   - 理想上只回写脏页（RISC-V PTE 的 D 位）
   - 但 `mmaptest` 不强制要求“非脏页不回写”，不检查 D 位也可通过。
7. 修改 `exit()`：退出时清理全部 VMA，并处理共享映射回写。
8. 修改 `fork()`：让子进程继承父进程 VMA 与文件引用：
   - 记得给 VMA 的 `struct file*` 增加引用计数
   - 子进程缺页时分配自己的物理页即可，不必与父进程共享页。

完成后运行 `usertests -q` 确认无回归。

---

## 提交实验

### 记录耗时

创建 `time.txt`，写入一个整数（实验耗时小时数），然后 `git add` + `git commit`。

### 回答问题

若本实验有问答题，将答案写入 `answers-*.txt`，然后提交。

### 提交方式

作业通过 Gradescope 提交。你需要 MIT Gradescope 账号；课程加入码见 Piazza。如需帮助可参考[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

运行 `make zipball` 生成 `lab.zip` 后上传。

若存在未提交或未跟踪文件，可能看到：

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

确保解题所需文件都已被跟踪（可用 `git add {filename}`）。

- 提交前运行 `make grade`
- `make zipball` 前先提交源码修改
- Gradescope 实验分数即最终实验成绩

---

## 可选挑战

- 当两个进程映射同一文件时共享物理页（如 fork 场景）；需要物理页引用计数。
- 消除 mmap 与 buffer cache 的双重缓存：直接复用缓存中的物理页（需 `BSIZE=4096`、pin 块、管理引用计数）。这还可改善 read/write 与 mmap 的一致性。可参考论文 [unified buffer cache](https://www.usenix.org/legacy/publications/library/proceedings/usenix2000/freenix/full_papers/silvers/silvers.pdf)。
- 消除懒分配实现与 mmap 实现间冗余（提示：把懒分配区域也建模为 VMA）。
- 修改 `exec`：用 VMA 表示二进制不同段，实现按需加载执行文件。
- 实现 page-out/page-in：物理内存紧张时换出，访问时再换入。

---

*如有关于 6.1810 的问题或建议，请邮件联系课程组：61810-staff@lists.csail.mit.edu。*
