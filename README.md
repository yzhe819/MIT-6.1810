# MIT 6.1810 Operating System Engineering — Lab Notes

[English](./README.md) · [中文](./README_zh.md)

## Course Overview

MIT 6.1810 Operating System Engineering is an operating systems course offered by MIT's Department of Electrical Engineering and Computer Science, formerly known as MIT 6.828. Built on the RISC-V architecture and the xv6 teaching operating system, the course systematically covers the core mechanisms and design principles of modern operating systems.

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

If you are new to this course, this is the best place to start. Everything related to the labs can be found here.

A complete index of all labs, including official documentation and code repositories:

### -> [Lab Index](./index.md)

---

> The xv6 operating system is developed and maintained by MIT PDOS Lab.
>
> This repository contains personal notes and solutions for educational purposes only.