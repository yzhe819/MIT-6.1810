# MIT 6.1810 编程指北

[English](./README.md) · [中文](./README_zh.md)

## 课程介绍

MIT 6.1810 Operating System Engineering 是麻省理工学院计算机科学系的操作系统课程，其前身为广为人知的 MIT 6.828。课程以 RISC-V 架构和教学操作系统 xv6 为基础，系统讲解现代操作系统的核心机制与设计思想。

课程的核心部分是 Lab 实验。每个实验都会围绕 xv6 的某个子系统展开，通过实现新功能或改进现有机制，加深对操作系统原理的理解。

> [!IMPORTANT]
> 本文所有内容均基于 **MIT 6.1810 Operating System Engineering (Fall 2025)** 课程及其对应的 **xv6-labs-2025** 实验仓库编写。
>
> MIT 6.1810 的 Lab 内容会随着学期更新而发生变化，不同年份的实验要求、测试脚本甚至题目设计均可能存在差异。因此，本文中的代码实现、源码分析与实验说明仅保证适用于 **Fall 2025** 版本。

---

## 快速导航

如果你是初次接触本课程，建议从这里开始。一切关于 Lab 的内容均可在此找到。

各实验详细索引，包含官方文档、中文汉化及代码仓库链接：

### -> [Lab 目录](./index_zh.md)

---

## 项目结构

所有 Lab 基于同一个代码仓库，每个实验对应一个独立分支：

```text
yzhe819/MIT-6.1810/
│
├── main      Documentation & lab index
│
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

## 课程资源

* 课程主页：https://pdos.csail.mit.edu/6.1810/2025/index.html
* xv6 主页：https://pdos.csail.mit.edu/6.1810/2025/xv6.html
* xv6 Book：https://pdos.csail.mit.edu/6.1810/2025/xv6/book-riscv-rev5.pdf
* xv6 Source Booklet：https://pdos.csail.mit.edu/6.1810/2025/xv6/xv6-src-booklet-rev5.pdf

---


> The xv6 operating system is developed and maintained by MIT PDOS Lab.
>
> This repository only contains personal notes and solutions for educational purposes.