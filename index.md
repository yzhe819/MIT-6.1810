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

### Lab 2 — System Calls（2d）

**Overview:** Understanding the system call mechanism and the transition between user mode and kernel mode.

**Difficulty:** `using gdb` (easy) · `sandbox a command` (moderate) · `sandbox with allowed pathnames` (easy) · `attack xv6` (moderate)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/syscall.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab2-xv6-system-calls/lab2-xv6-system-calls.md) · [Solution](https://github.com/yzhe819/MIT-6.1810/blob/main/lab2-xv6-system-calls/lab2-xv6-system-calls-solution.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/syscall)

### Lab 3 — Page Tables（6h）

**Overview:** Explore RISC-V page tables, implement page-table inspection/printing, speed up `getpid()` with shared read-only mapping, and add superpage support in xv6.

**Difficulty:** `inspect a user-process page table` (easy) · `speed up system calls` (easy) · `print a page table` (easy) · `use superpages` (moderate/hard)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/pgtbl.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab3-xv6-page-tables/lab3-xv6-page-tables.md) · [Solution](https://github.com/yzhe819/MIT-6.1810/blob/main/lab3-xv6-page-tables/lab3-xv6-page-tables-solution.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/pgtbl)

### Lab 4 — Traps

**Overview:** Understand trap-based system call flow, implement kernel backtrace, and add periodic user-level alarms (`sigalarm`/`sigreturn`).

**Difficulty:** `risc-v assembly` (easy) · `backtrace` (moderate) · `alarm` (hard)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/traps.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab4-xv6-traps/lab4-xv6-traps.md) · [Solution](https://github.com/yzhe819/MIT-6.1810/blob/main/lab4-xv6-traps/lab4-xv6-traps-solution.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/traps)

### Lab 5 — Copy-on-Write Fork

**Overview:** Implement COW fork in xv6 by sharing pages initially, handling write faults lazily, and maintaining per-page reference counts correctly.

**Difficulty:** `implement copy-on-write fork`

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/cow.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab5-xv6-copy-on-write/lab5-xv6-copy-on-write.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/cow)


### Lab 6 — Network Driver

**Overview:** Implement E1000 TX/RX driver paths and complete UDP receive path (`ip_rx`, `sys_recv`, `sys_bind`) in xv6 networking stack.

**Difficulty:** `part one: nic` · `part two: udp receive`

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/net.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab6-xv6-network-driver/lab6-xv6-network-driver.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/net)

### Lab 7 — Locks

**Overview:** Redesign the allocator to reduce lock contention (`kalloc`/`kfree` per-CPU freelists + stealing), and implement a writer-priority read-write spinlock in xv6.

**Difficulty:** `memory allocator` (hard) · `read-write lock` (moderate/hard)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/lock.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab7-xv6-locks/lab7-xv6-locks.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/lock)

### Lab 8 — File System

**Overview:** Extend xv6 file system with doubly-indirect inode blocks for large files and implement symbolic links (`symlink`) with `open()` follow/no-follow semantics.

**Difficulty:** `large files` (hard) · `symbolic links` (moderate)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/fs.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab8-xv6-file-system/lab8-xv6-file-system.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/fs)

### Lab 9 — mmap

**Overview:** Implement lazy file-backed memory mapping in xv6 with `mmap`/`munmap`, VMA tracking, page-fault-driven allocation/load, shared-map writeback, and fork/exit handling.

**Difficulty:** `mmap/munmap` (hard) · `VMA + page-fault path` (hard)

[Official Docs](https://pdos.csail.mit.edu/6.1810/2025/labs/mmap.html) · [Lab Questions](https://github.com/yzhe819/MIT-6.1810/blob/main/lab9-xv6-mmap/lab9-xv6-mmap.md) · [Code](https://github.com/yzhe819/MIT-6.1810/tree/mmap)
