# MIT 6.1810 OS Engineering — Lab Notes

[English](./README.md) · [中文](./README_zh.md)

## Course Overview

MIT 6.1810 is an operating systems course offered by MIT's Department of Electrical Engineering and Computer Science, formerly known as MIT 6.828/MIT 6.S081. Built on the RISC-V architecture and the xv6 teaching operating system, the course systematically covers the core mechanisms and design principles of modern operating systems.

> [!IMPORTANT]
> All content in this repository is based on **MIT 6.1810 Operating System Engineering (Fall 2025)** and its corresponding **xv6-labs-2025** lab repository.

---

## Why This Repo?

Most reference materials for MIT's operating system courses (6.828 / 6.S081 / 6.1810) are based on versions from 2018–2022. Since then, the Labs have undergone significant changes, making many existing write-ups outdated or no longer applicable. 

So I thought — why not write one myself? A few things that make this repo different:

- *Up-to-date Labs (2025 Fall)*
- *Code and notes together*
- *Optional Challenges included*
- *A returning perspective*

---

## Quick Start

If you are new to this course, this is the best place to start. Everything related to the labs can be found here.

### 实验中文题解 / Chinese Solutions

- [Lab 1 — Unix Utilities｜Unix 实用工具](./lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-solution-zh.md)
- [Lab 2 — System Calls｜系统调用](./lab2-xv6-system-calls/lab2-xv6-system-calls-solution-zh.md)
- [Lab 3 — Page Tables｜页表](./lab3-xv6-page-tables/lab3-xv6-page-tables-solution-zh.md)
- [Lab 4 — Traps｜陷阱](./lab4-xv6-traps/lab4-xv6-traps-solution-zh.md)
- [Lab 5 — Copy-on-Write Fork｜写时复制](./lab5-xv6-copy-on-write/lab5-xv6-copy-on-write-solution-zh.md)
- [Lab 6 — Network Driver｜网络驱动](./lab6-xv6-network-driver/lab6-xv6-network-driver-solution-zh.md)
- [Lab 7 — Locks｜锁](./lab7-xv6-locks/lab7-xv6-locks-solution-zh.md)
- [Lab 8 — File System｜文件系统](./lab8-xv6-file-system/lab8-xv6-file-system-solution-zh.md)
- [Lab 9 — mmap｜内存映射](./lab9-xv6-mmap/lab9-xv6-mmap-solution-zh.md)

### Lab Solutions (English / AI Version)

- [Lab 1 — Unix Utilities](./lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-solution.md)
- [Lab 2 — System Calls](./lab2-xv6-system-calls/lab2-xv6-system-calls-solution.md)
- [Lab 3 — Page Tables](./lab3-xv6-page-tables/lab3-xv6-page-tables-solution.md)
- [Lab 4 — Traps](./lab4-xv6-traps/lab4-xv6-traps-solution.md)
- [Lab 5 — Copy-on-Write Fork](./lab5-xv6-copy-on-write/lab5-xv6-copy-on-write-solution.md)
- [Lab 6 — Network Driver (AI Version)](./lab6-xv6-network-driver/lab6-xv6-network-driver-solution.md)
- [Lab 7 — Locks (AI Version)](./lab7-xv6-locks/lab7-xv6-locks-solution.md)
- [Lab 8 — File System (AI Version)](./lab8-xv6-file-system/lab8-xv6-file-system-solution.md)
- [Lab 9 — mmap (AI Version)](./lab9-xv6-mmap/lab9-xv6-mmap-solution.md)

A complete index of all labs, including official documentation and code repositories:

- [中文笔记完整索引](./index_zh.md)
- [Lab Directory (English)](./index.md)

---

## Repository Structure

All labs share a single repository, with each lab on its own branch:

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

## Course Resources

* Course Homepage: https://pdos.csail.mit.edu/6.1810/2025/index.html
* xv6 Homepage: https://pdos.csail.mit.edu/6.1810/2025/xv6.html
* xv6 Book: https://pdos.csail.mit.edu/6.1810/2025/xv6/book-riscv-rev5.pdf
* xv6 Source Booklet: https://pdos.csail.mit.edu/6.1810/2025/xv6/xv6-src-booklet-rev5.pdf

---

> The xv6 operating system is developed and maintained by MIT PDOS Lab.
>
> This repository contains personal notes and solutions for educational purposes only.