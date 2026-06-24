# MIT 6.1810 编程指北

## 课程介绍

MIT 6.1810 Operating System Engineering 是麻省理工学院计算机科学系的操作系统课程，其前身为广为人知的 MIT 6.828。课程以 RISC-V 架构和教学操作系统 xv6 为基础，系统讲解现代操作系统的核心机制与设计思想。

课程内容涵盖系统调用、进程管理、虚拟内存、异常与中断、文件系统、并发控制、网络驱动等主题，重点理解操作系统如何实现抽象、隔离与资源管理，并为上层应用程序提供运行环境。

课程的核心部分是 Lab 实验。每个实验都会围绕 xv6 的某个子系统展开，通过实现新功能或改进现有机制，加深对操作系统原理的理解。

> [!IMPORTANT]
> 本文所有内容均基于 **MIT 6.1810 Operating System Engineering (Fall 2025)** 课程及其对应的 **xv6-labs-2025** 实验仓库编写。
>
> MIT 6.1810 的 Lab 内容会随着学期更新而发生变化，不同年份的实验要求、测试脚本甚至题目设计均可能存在差异。因此，本文中的代码实现、源码分析与实验说明仅保证适用于 **Fall 2025** 版本。

---

## 课程资源

* 课程主页：https://pdos.csail.mit.edu/6.1810/2025/index.html
* xv6 主页：https://pdos.csail.mit.edu/6.1810/2025/xv6.html
* xv6 Book：https://pdos.csail.mit.edu/6.1810/2025/xv6/book-riscv-rev5.pdf
* xv6 Source Booklet：https://pdos.csail.mit.edu/6.1810/2025/xv6/xv6-src-booklet-rev5.pdf

---

## 项目结构

所有 Lab 基于同一个代码仓库，每个实验对应一个独立分支：

```text
yzhe819/MIT-6.1810/
├── util      Lab 1: Unix Utilities        ✓ 2026-06-25
├── syscall   Lab 2: System Calls
├── pgtbl     Lab 3: Page Tables
├── traps     Lab 4: Traps
├── cow       Lab 5: Copy-on-Write Fork
├── thread    Lab 6: Multithreading
├── net       Lab 7: Network Driver
├── lock      Lab 8: Locks
├── fs        Lab 9: File System
└── mmap      Lab 10: Mmap
```

---

## Lab 指引

官方难度参考：

* **easy**：小于 1 小时，通常是为后续练习热身
* **moderate**：1～2 小时
* **hard**：大于 2 小时，代码量不多，但细节极难写对

## Lab 0 — Tools｜搭建实验环境

**目标简述：** 配好实验环境和代码仓库。Lab 仓库与 xv6 官方仓库基本一致，但额外提供了测试与评分代码，用于自动验证实验结果。

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/tools.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/6.1810-tools-setup-guide/6.1810-tools-setup-guide_zh.md)

## Lab 1 — Unix Utilities｜Unix 实用工具（2h）

**目标简述：** 实现若干用户态程序及 Unix 实用工具，熟悉 xv6 的开发环境与系统调用的基本使用方式。

**实验难度：** `sleep` (easy) · `sixfive` (moderate) · `memdump` (easy) · `find` (moderate) · `exec` (moderate)

[官方文档](https://pdos.csail.mit.edu/6.1810/2025/labs/util.html) · [中文汉化](https://github.com/yzhe819/MIT-6.1810/blob/main/lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-zh.md) · [中文题解](https://github.com/yzhe819/MIT-6.1810/blob/main/lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-zh.md) · [代码仓库](https://github.com/yzhe819/MIT-6.1810/tree/util)


---


> The xv6 operating system is developed and maintained by MIT PDOS Lab.
>
> This repository only contains personal notes and solutions for educational purposes.