# MIT 6.1810 编程指北

[English](./index.md) · [中文](./index_zh.md)

### Lab 指引

官方难度参考：

* **easy**：小于 1 小时，通常是为后续练习热身
* **moderate**：1～2 小时
* **hard**：大于 2 小时，代码量不多，但细节极难写对

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/guidance.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/6.1810-lab-guidance/6.1810-lab-guidance-zh.md)

### Lab 0 — Tools｜搭建实验环境

**目标简述：** 配好实验环境和代码仓库。Lab 仓库与 xv6 官方仓库基本一致，但额外提供了测试与评分代码，用于自动验证实验结果。

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/tools.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/6.1810-tools-setup-guide/6.1810-tools-setup-guide_zh.md)

### Lab 1 — Unix Utilities｜Unix 实用工具（2h）

**目标简述：** 实现若干用户态程序及 Unix 实用工具，熟悉 xv6 的开发环境与系统调用的基本使用方式。

**实验难度：** `sleep` (easy) · `sixfive` (moderate) · `memdump` (easy) · `find` (moderate) · `exec` (moderate)

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/util.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-zh.md) · [中文题解](https://github.com/yzhe819/MIT-6.1810/blob/main/lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-solution-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/util)

### Lab 2 — System Calls｜系统调用（2d）

**目标简述：** 理解系统调用机制，以及用户态（User Mode）与内核态（Kernel Mode）之间的切换过程。

**实验难度：** `using gdb` (easy) · `sandbox a command` (moderate) · `sandbox with allowed pathnames` (easy) · `attack xv6` (moderate)

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/syscall.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab2-xv6-system-calls/lab2-xv6-system-calls-zh.md) · [中文题解](https://github.com/yzhe819/MIT-6.1810/blob/main/lab2-xv6-system-calls/lab2-xv6-system-calls-solution-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/syscall)

### Lab 3 — Page Tables｜页表（6h）

**目标简述：** 探索 RISC-V 页表机制，完成页表检查/打印、通过共享只读页优化 `getpid()`，并在 xv6 中实现 superpage（大页）支持。

**实验难度：** `inspect a user-process page table` (easy) · `speed up system calls` (easy) · `print a page table` (easy) · `use superpages` (moderate/hard)

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/pgtbl.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab3-xv6-page-tables/lab3-xv6-page-tables-zh.md) · [中文题解](https://github.com/yzhe819/MIT-6.1810/blob/main/lab3-xv6-page-tables/lab3-xv6-page-tables-solution-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/pgtbl)

### Lab 4 — Traps｜陷阱

**目标简述：** 理解基于 trap 的系统调用流程，实现内核 backtrace，并加入周期性用户态 alarm（`sigalarm`/`sigreturn`）机制。

**实验难度：** `risc-v assembly` (easy) · `backtrace` (moderate) · `alarm` (hard)

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/traps.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab4-xv6-traps/lab4-xv6-traps-zh.md) · [中文题解](https://github.com/yzhe819/MIT-6.1810/blob/main/lab4-xv6-traps/lab4-xv6-traps-solution-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/traps)

### Lab 5 — Copy-on-Write Fork｜写时复制

**目标简述：** 在 xv6 中实现 COW fork：初始共享物理页、写入时按需复制，并正确维护物理页引用计数。

**实验难度：** `implement copy-on-write fork`

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/cow.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab5-xv6-copy-on-write/lab5-xv6-copy-on-write-zh.md) · [中文题解](https://github.com/yzhe819/MIT-6.1810/blob/main/lab5-xv6-copy-on-write/lab5-xv6-copy-on-write-solution-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/cow)


### Lab 6 — Network Driver｜网络驱动

**目标简述：** 实现 E1000 收发驱动路径，并补全 xv6 网络栈 UDP 接收路径（`ip_rx`、`sys_recv`、`sys_bind`）。

**实验难度：** `part one: nic` · `part two: udp receive`

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/net.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab6-xv6-network-driver/lab6-xv6-network-driver-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/net)

### Lab 7 — Locks｜锁

**目标简述：** 重构内存分配器以降低锁竞争（`kalloc`/`kfree` 每核空闲链表 + stealing），并在 xv6 中实现写者优先的读写自旋锁。

**实验难度：** `memory allocator` (hard) · `read-write lock` (moderate/hard)

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/lock.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab7-xv6-locks/lab7-xv6-locks-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/lock)

### Lab 8 — File System｜文件系统

**目标简述：** 为 xv6 文件系统加入二级间接块支持以实现大文件，并实现符号链接（`symlink`）及 `open()` 的跟随/不跟随语义。

**实验难度：** `large files` (hard) · `symbolic links` (moderate)

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/fs.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab8-xv6-file-system/lab8-xv6-file-system-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/fs)

### Lab 9 — mmap｜内存映射

**目标简述：** 在 xv6 中实现基于文件的懒加载内存映射（`mmap`/`munmap`），包括 VMA 管理、缺页触发分配与读入、共享映射回写，以及 fork/exit 下的映射处理。

**实验难度：** `mmap/munmap` (hard) · `VMA + 缺页处理路径` (hard)

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/mmap.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab9-xv6-mmap/lab9-xv6-mmap-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/mmap)
