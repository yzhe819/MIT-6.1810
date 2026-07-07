# MIT 6.1810 Operating System Engineering — Lab Notes

[English](./index.md) · [中文](./index_zh.md)

### Lab Guide

Official difficulty reference:

* **easy**: under 1 hour, typically a warm-up for subsequent exercises
* **moderate**: 1–2 hours
* **hard**: over 2 hours — not much code, but the details are notoriously difficult to get right

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/guidance.html) · [Full Description](https://github.com/yzhe819/MIT-6.1810/blob/main/6.1810-lab-guidance/6.1810-lab-guidance.md)

### Lab 0 — Tools｜Environment Setup

**Overview:** Set up the development environment and lab repository. The lab repo closely mirrors the official xv6 repository, with additional test and grading scripts for automated verification.

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/tools.html) · [Setup Guide](https://github.com/yzhe819/MIT-6.1810/blob/main/6.1810-tools-setup-guide/6.1810-tools-setup-guide.md)

### Lab 1 — Unix Utilities (2h)

**Overview:** Implement several user-space programs and Unix utilities to get familiar with the xv6 development workflow and basic system call usage.

**Difficulty:** `sleep` (easy) · `sixfive` (moderate) · `memdump` (easy) · `find` (moderate) · `exec` (moderate)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/util.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab1-xv6-unix-utilities/lab1-xv6-unix-utilities.md) · [Solution](https://github.com/yzhe819/MIT-6.1810/blob/main/lab1-xv6-unix-utilities/lab1-xv6-unix-utilities-solution.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/util)

### Lab 2 — System Calls

**Overview:** Understanding the system call mechanism and the transition between user mode and kernel mode.

**Difficulty:** `using gdb` (easy) · `sandbox a command` (moderate) · `sandbox with allowed pathnames` (easy) · `attack xv6` (moderate)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/syscall.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab2-xv6-system-calls/lab2-xv6-system-calls.md) · [Solution](https://github.com/yzhe819/MIT-6.1810/blob/main/lab2-xv6-system-calls/lab2-xv6-system-calls-solution.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/syscall)

### Lab 3 — Page Tables

**Overview:** Explore RISC-V page tables, implement page-table inspection/printing, speed up `getpid()` with shared read-only mapping, and add superpage support in xv6.

**Difficulty:** `inspect a user-process page table` · `speed up system calls` · `print a page table` · `use superpages`

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/pgtbl.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab3-xv6-page-tables/lab3-xv6-page-tables.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/pgtbl)