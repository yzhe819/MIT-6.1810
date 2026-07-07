# MIT 6.1810 Operating System Engineering — Lab Notes

[English](./README.md) · [中文](./README_zh.md)

## Course Overview

MIT 6.1810 is an operating systems course offered by MIT's Department of Electrical Engineering and Computer Science, formerly known as MIT 6.828. Built on the RISC-V architecture and the xv6 teaching operating system, the course systematically covers the core mechanisms and design principles of modern operating systems.

The heart of the course is its Lab assignments. Each lab targets a specific xv6 subsystem, deepening understanding of OS principles by implementing new features or improving existing mechanisms.

> [!IMPORTANT]
> All content in this repository is based on **MIT 6.1810 Operating System Engineering (Fall 2025)** and its corresponding **xv6-labs-2025** lab repository.

---

## Why This Repo?

Most reference materials for MIT's operating system courses (6.828 / 6.S081 / 6.1810) are based on versions from 2018–2022. Since then, the Labs have undergone significant changes, making many existing write-ups outdated or no longer applicable. 

So I thought — why not write one myself?

A few things that make this repo different:

- **Up-to-date Labs (2025 Fall)** · Based on the Fall 2025 version of the course, aligned with the current lab assignments.
- **Bilingual write-ups** · Notes maintained in both Chinese and English, hoping to make the content accessible to a wider audience.
- **Code and notes together** · Beyond just sharing working code, each write-up documents the reasoning, pitfalls, and debugging process — written to help readers actually understand, not just copy.
- **Optional Challenges included** · Goes beyond the required exercises — all optional challenge problems are attempted as well.
- **A returning perspective** · The author previously completed earlier versions of 6.828 and 6.S081, and revisits the new Labs years later to document what has changed and what hasn't.

---

## Quick Start

If you are new to this course, this is the best place to start. Everything related to the labs can be found here.

A complete index of all labs, including official documentation and code repositories:

### -> [Lab Directory](./index.md)

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
├── pgtbl     Lab 3: Page Tables           ✓ 2026-07-07
├── traps     Lab 4: Traps
├── cow       Lab 5: Copy-on-Write Fork
├── thread    Lab 6: Multithreading
├── net       Lab 7: Network Driver
├── lock      Lab 8: Locks
├── fs        Lab 9: File System
└── mmap      Lab 10: Mmap
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