# MIT 6.1810 操作系统 编程指北

[English](./README.md) · [中文](./README_zh.md)

## 课程介绍

MIT 6.1810 是麻省理工学院计算机科学系的操作系统课程，其前身为广为人知的 MIT 6.828/MIT 6.S081。课程以 RISC-V 架构和教学操作系统 xv6 为基础，系统讲解现代操作系统的核心机制与设计思想。

> [!IMPORTANT]
> 本文所有内容均基于 **MIT 6.1810 Operating System Engineering (Fall 2025)** 课程及其对应的 **xv6-labs-2025** 实验仓库编写。

---

## 为什么有这个仓库？

网络上大多数 MIT 操作系统课程（6.828 / 6.S081 / 6.1810）的参考资料停留在 2018–2022 年版本，而课程 Lab 经历了较大改动，旧有题解已不完全适用。所以笔者想，那就我来写一份吧。

本仓库的几个特点：

- *最新版 Labs (2025 秋季)*
- *代码与笔记结合*
- *包含所有选做挑战题*
- *有经验的回归视角*

---

## 快速导航

如果你是初次接触本课程，建议从这里开始。一切关于 Lab 的内容均可在此找到。

### 实验中文题解

- [Lab 1 — Unix Utilities｜Unix 实用工具](./lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-solution-zh.md)
- [Lab 2 — System Calls｜系统调用](./lab2-xv6-system-calls/lab2-xv6-system-calls-solution-zh.md)
- [Lab 3 — Page Tables｜页表](./lab3-xv6-page-tables/lab3-xv6-page-tables-solution-zh.md)
- [Lab 4 — Traps｜陷阱](./lab4-xv6-traps/lab4-xv6-traps-solution-zh.md)
- [Lab 5 — Copy-on-Write Fork｜写时复制](./lab5-xv6-copy-on-write/lab5-xv6-copy-on-write-solution-zh.md)
- [Lab 6 — Network Driver｜网络驱动](./lab6-xv6-network-driver/lab6-xv6-network-driver-solution-zh.md)
- [Lab 7 — Locks｜锁](./lab7-xv6-locks/lab7-xv6-locks-solution-zh.md)
- [Lab 8 — File System｜文件系统](./lab8-xv6-file-system/lab8-xv6-file-system-solution-zh.md)
- [Lab 9 — mmap｜内存映射](./lab9-xv6-mmap/lab9-xv6-mmap-solution-zh.md)

### 实验英文题解 / Lab Solutions (English)

- [Lab 1 — Unix Utilities](./lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-solution.md)
- [Lab 2 — System Calls](./lab2-xv6-system-calls/lab2-xv6-system-calls-solution.md)
- [Lab 3 — Page Tables](./lab3-xv6-page-tables/lab3-xv6-page-tables-solution.md)
- [Lab 4 — Traps](./lab4-xv6-traps/lab4-xv6-traps-solution.md)
- [Lab 5 — Copy-on-Write Fork](./lab5-xv6-copy-on-write/lab5-xv6-copy-on-write-solution.md)
- [Lab 6 — Network Driver (AI Version)](./lab6-xv6-network-driver/lab6-xv6-network-driver-solution.md)
- [Lab 7 — Locks (AI Version)](./lab7-xv6-locks/lab7-xv6-locks-solution.md)
- [Lab 8 — File System (AI Version)](./lab8-xv6-file-system/lab8-xv6-file-system-solution.md)
- [Lab 9 — mmap (AI Version)](./lab9-xv6-mmap/lab9-xv6-mmap-solution.md)

各实验详细索引（包含官方文档、环境搭建、中文汉化题目及代码仓库）：

- [中文笔记完整索引](./index_zh.md)
- [Lab Directory (English)](./index.md)

---

## 项目结构

所有 Lab 基于同一个代码仓库，每个实验对应一个独立分支：

```text
yzhe819/MIT-6.1810/
│
├── main      Documentation & lab index
│
├── util      Lab 1: Unix Utilities        ✓ 2026-06-25
├── syscall   Lab 2: System Calls          ✓ 2026-06-29
├── pgtbl     Lab 3: Page Tables           ✓ 2026-07-08
├── traps     Lab 4: Traps                 ✓ 2026-07-10
├── cow       Lab 5: Copy-on-Write Fork    ✓ 2026-07-12
├── net       Lab 6: Network Driver        ✓ 2026-07-12
├── lock      Lab 7: Locks                 ✓ 2026-07-18
├── fs        Lab 8: File System           ✓ 2026-07-18
└── mmap      Lab 9: Mmap                  ✓ 2026-07-19
```

---

## 课程资源

* 课程主页：https://pdos.csail.mit.edu/6.1810/2025/index.html
* xv6 主页：https://pdos.csail.mit.edu/6.1810/2025/xv6.html
* xv6 Book：https://pdos.csail.mit.edu/6.1810/2025/xv6/book-riscv-rev5.pdf
* xv6 Source Booklet：https://pdos.csail.mit.edu/6.1810/2025/xv6/xv6-src-booklet-rev5.pdf

---


> The xv6 operating system is developed and maintained by MIT PDOS Lab.
>
> This repository only contains personal notes and solutions for educational purposes.