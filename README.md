# MIT 6.1810 Operating System Engineering — Lab Notes

[English](./README.md) · [中文](./README_zh.md)

## Course Overview

MIT 6.1810 Operating System Engineering is an operating systems course offered by MIT's Department of Electrical Engineering and Computer Science, formerly known as MIT 6.828. Built on the RISC-V architecture and the xv6 teaching operating system, the course systematically covers the core mechanisms and design principles of modern operating systems.

Topics include system calls, process management, virtual memory, traps and interrupts, file systems, concurrency, and network drivers, with an emphasis on understanding how an OS provides abstraction, isolation, and resource management for user-space programs.

The heart of the course is its Lab assignments. Each lab targets a specific xv6 subsystem, deepening understanding of OS principles by implementing new features or improving existing mechanisms.

> [!IMPORTANT]
> All content in this repository is based on **MIT 6.1810 Operating System Engineering (Fall 2025)** and its corresponding **xv6-labs-2025** lab repository.
>
> Lab content is updated each semester. Requirements, test scripts, and problem designs may differ across years. All code, source analysis, and lab write-ups here are written for the **Fall 2025** version only.

---

## Course Resources

* Course Homepage: https://pdos.csail.mit.edu/6.1810/2025/index.html
* xv6 Homepage: https://pdos.csail.mit.edu/6.1810/2025/xv6.html
* xv6 Book: https://pdos.csail.mit.edu/6.1810/2025/xv6/book-riscv-rev5.pdf
* xv6 Source Booklet: https://pdos.csail.mit.edu/6.1810/2025/xv6/xv6-src-booklet-rev5.pdf

---

## Repository Structure

All labs share a single repository, with each lab on its own branch:

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

## Lab Guide

Official difficulty reference:

* **easy**: under 1 hour, typically a warm-up for subsequent exercises
* **moderate**: 1–2 hours
* **hard**: over 2 hours — not much code, but the details are notoriously difficult to get right

## Lab 0 — Tools｜Environment Setup

**Overview:** Set up the development environment and lab repository. The lab repo closely mirrors the official xv6 repository, with additional test and grading scripts for automated verification.

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/tools.html) · [Setup Guide](https://github.com/yzhe819/MIT-6.1810/blob/main/6.1810-tools-setup-guide/6.1810-tools-setup-guide.md)

## Lab 1 — Unix Utilities (2h)

**Overview:** Implement several user-space programs and Unix utilities to get familiar with the xv6 development workflow and basic system call usage.

**Difficulty:** `sleep` (easy) · `sixfive` (moderate) · `memdump` (easy) · `find` (moderate) · `exec` (moderate)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/util.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab1-xv6-unix-utilities/lab1-xv6-unix-utilities.md) · [Solution](https://github.com/yzhe819/MIT-6.1810/blob/main/lab1-xv6-unix-utilities/lab1-xv6-unix-utilities.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/util)

---

> The xv6 operating system is developed and maintained by MIT PDOS Lab.
>
> This repository contains personal notes and solutions for educational purposes only.