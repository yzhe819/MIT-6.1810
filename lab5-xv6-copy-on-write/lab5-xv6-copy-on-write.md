# Lab 5: Copy-on-Write Fork for xv6

[English](./lab5-xv6-copy-on-write.md) · [中文](./lab5-xv6-copy-on-write-zh.md)

Virtual memory provides a level of indirection: the kernel can intercept memory references by marking PTEs invalid or read-only, leading to page faults, and can change what addresses mean by modifying PTEs. There is a saying in computer systems that any systems problem can be solved with a level of indirection. This lab explores one example: copy-on-write fork.

To start the lab, switch to the `cow` branch:

```bash
$ git fetch
$ git checkout cow
$ make clean
```

---

## The Problem

The `fork()` system call in xv6 copies all of the parent process's user-space memory into the child. If the parent is large, copying can take a long time. Worse, this work is often wasted: `fork()` is commonly followed by `exec()` in the child, which discards the copied memory, usually without using most of it. On the other hand, if parent and child both use a copied page and one of them writes it, the copy is truly needed.

---

## The Solution

Your goal in implementing copy-on-write (COW) `fork()` is to defer allocating and copying physical memory pages until the copies are actually needed (if ever).

COW `fork()` creates only a page table for the child, with user-memory PTEs pointing to the parent's physical pages. COW `fork()` marks all user PTEs in both parent and child as read-only. When either process tries to write one of these COW pages, the CPU triggers a page fault. The kernel page-fault handler detects this case, allocates a new physical page for the faulting process, copies the original page into it, and updates the faulting process's PTE to point to the new page with write permission restored. After the handler returns, the process can write to its private copy.

COW `fork()` also makes physical-page freeing trickier. A physical page may be referenced by multiple process page tables, and should be freed only when the last reference disappears. In a small kernel like xv6 this bookkeeping is manageable, but in production kernels it is easy to get wrong; see [Patching until the COWs come home](https://lwn.net/Articles/849638/).

---

## Implement copy-on-write fork

Your task is to implement copy-on-write `fork` in the xv6 kernel. You are done when your modified kernel executes both `cowtest` and `usertests -q` successfully.

To help testing, xv6 provides `cowtest` (source: `user/cowtest.c`). `cowtest` runs multiple tests; even the first one fails on unmodified xv6:

```bash
$ cowtest
simple: fork() failed
$
```

The `simple` test allocates more than half of available physical memory and then calls `fork()`. The fork fails because there is not enough free physical memory to provide the child a full copy of the parent's memory.

When complete, your kernel should pass all tests in both `cowtest` and `usertests -q`:

```bash
$ cowtest
simple: ok
simple: ok
three: ok
three: ok
three: ok
file: ok
forkfork: ok
ALL COW TESTS PASSED
$ usertests -q
...
ALL TESTS PASSED
$
```

A reasonable plan:

1. Modify `uvmcopy()` to map parent physical pages into the child instead of allocating new pages. Clear `PTE_W` in both parent and child PTEs for pages that originally had `PTE_W` set.
2. Modify `vmfault()` to recognize write page faults on COW pages. When such a fault occurs on a page that was originally writable, allocate a new page via `kalloc()`, copy old content to new page, and install the new page in the faulting PTE with `PTE_W` set. Pages that were originally read-only (e.g. text segment pages) must remain read-only/shared; a process attempting to write such a page should be killed.
3. Ensure each physical page is freed only when the last PTE reference disappears. A common approach is to maintain a per-physical-page reference count for user page-table references. Set it to 1 when `kalloc()` allocates a page; increment on share during `fork`; decrement when any process unmaps it. `kfree()` should return the page to free list only when count reaches 0. A fixed-size integer array is acceptable; design a proper indexing scheme and array size (for example, index by physical address / 4096, with size based on highest physical address ever put on free list by `kinit()` in `kalloc.c`). You may modify `kalloc.c` (`kalloc()`/`kfree()`) to maintain this count.
4. Modify `copyout()` to use the same COW handling logic when it encounters a COW page.

Some hints:

- You may need a way to record whether a PTE is COW. You can use RISC-V PTE `RSW` bits (reserved for software).
- `usertests -q` covers scenarios not in `cowtest`, so run both.
- Helpful flag macros/definitions are near the end of `kernel/riscv.h`.
- If a COW page fault occurs and there is no free memory, kill the process.

---

## Submit the Lab

### Time Spent

Create a new file `time.txt` containing a single integer: the number of hours you spent on the lab. `git add` and `git commit` the file.

### Answers

If this lab has questions, write your answers in `answers-*.txt`. `git add` and `git commit` these files.

### Submit

Assignment submissions are handled by Gradescope. You need an MIT Gradescope account. See Piazza for the course entry code. Use [this link](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code) if needed.

When ready, run `make zipball` to generate `lab.zip`. Upload that zip to the corresponding Gradescope assignment.

If you run `make zipball` with uncommitted changes or untracked files, you may see output like:

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

Inspect these lines and ensure all files needed by your solution are tracked (i.e., not in lines starting with `??`). Use `git add {filename}` to start tracking new files.

- Run `make grade` to ensure all tests pass. Gradescope autograder uses the same grading program.
- Commit modified source code before running `make zipball`.
- You can inspect submission status and download submitted code on Gradescope. The Gradescope lab grade is your final lab grade.

---

## Optional challenge exercise

- Measure how much your COW implementation reduces bytes copied by xv6 and the number of physical pages allocated. Find and exploit opportunities to reduce these numbers further.

---

*Questions or comments regarding 6.1810? Send e-mail to the course staff at 61810-staff@lists.csail.mit.edu.*
