# Lab 7: Locks

[English](./lab7-xv6-locks.md) · [中文](./lab7-xv6-locks-zh.md)

In this lab you'll gain experience in re-designing code to increase parallelism. A common symptom of poor parallelism on multi-core machines is high lock contention. Improving parallelism often requires changes to both data structures and locking strategy. You will do this for the xv6 memory allocator and for a read-write lock implementation.

Before writing code, read the following from the [xv6 book](xv6/book-riscv-rev5.pdf):

- Chapter 7: **Locking**
- Section 3.5: **Code: Physical memory allocator**

To start the lab:

```bash
$ git fetch
$ git checkout lock
$ make clean
```

---

## Memory Allocator (hard)

`user/kalloctest` stresses xv6's allocator: three processes repeatedly grow and shrink address spaces, causing many `kalloc`/`kfree` calls. These currently contend on `kmem.lock`.

`kalloctest` prints lock statistics, especially:

- `#acquire()`: number of acquires
- `#test-and-set`: failed lock attempts inside `acquire` loop (a rough contention measure)

Before optimization, output looks roughly like:

```text
$ kalloctest
start test1
...
lock: kmem: #test-and-set 18820 #acquire() 433058
...
tot= 18820
test1 FAIL
...
test4 FAIL m 59382 n 135309
```

Numbers and top-5 lock order will vary by machine.

The root cause is one global free list protected by one lock. Your task is to redesign allocator locking for parallelism:

1. Maintain a **per-CPU free list**, each with its own lock.
2. If a CPU's free list is empty, it must **steal** pages from another CPU's free list.

Stealing may cause occasional contention, but far less than a single global lock.

Requirements:

- Lock names must start with `"kmem"` (via `initlock`).
- Run `kalloctest` and confirm kmem contention drops significantly.
- Run `usertests sbrkmuch` to ensure memory can still be fully allocated/freed.
- Ensure `usertests -q` passes.
- `make grade` should pass kalloc-related tests.

Reference output after improvement (numbers differ):

```text
$ kalloctest
...
tot= 0
test1 OK
...
tot= 9046
test4 OK
```

### Hints

- Use `NCPU` from `kernel/param.h`.
- Let `freerange` give all free memory to the CPU that runs `freerange`.
- `cpuid()` is safe only with interrupts off; use `push_off()`/`pop_off()`.
- You may format lock names via `snprintf` in `kernel/sprintf.c` (or simply name all locks `"kmem"`).

Optional: run with race detector:

```bash
$ make clean
$ make KCSAN=1 qemu
$ kalloctest
```

`kalloctest` may fail under KCSAN due to slowdown, but there should be no race report.

If a race is reported, convert backtrace addresses via:

```bash
$ riscv64-linux-gnu-addr2line -e kernel/kernel
```

---

## Read-Write Lock (moderate/hard)

This half is independent of the allocator half.

In xv6, `sys_pause` and `sys_uptime` read global `ticks` under `tickslock`. This ensures correctness, but blocks concurrent readers unnecessarily. A read-write lock allows:

- Multiple readers simultaneously (if no writer)
- At most one writer
- No readers while a writer holds the lock

API (see `kernel/defs.h`):

```c
void initrwlock(struct rwspinlock*);
void read_acquire(struct rwspinlock*);
void read_release(struct rwspinlock*);
void write_acquire(struct rwspinlock*);
void write_release(struct rwspinlock*);
```

Important semantic requirement: **writers must not starve**. If any writer is waiting, subsequent readers must wait until writer acquires and releases.

Implement read-write spinlock in xv6 with the above API/semantics:

- Fill stub functions in `kernel/spinlock.c`.
- Likely update `struct rwspinlock` in `kernel/spinlock.h`.

Test with:

```text
$ rwlktest
rwspinlock_test: step 1
...
rwlktest: passed 3/3
```

Also ensure `usertests -q` still passes and `make grade` passes all lock-lab tests.

### Hints

- Start by reading `sys_rwlktest` in `kernel/spinlock.c`; you can implement in stages.
- Without modifications, `rwlktest` panics (`panic: acquire`) because ordinary spinlock assumes single owner; replace lock handling in write/read `_acquire_inner` and `_release_inner` with your rwlock logic.
- Carefully reason about interleavings, e.g. reader seeing no pending writer but writer arriving concurrently.
- GCC atomic builtins are useful: [gcc __atomic builtins](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html).

---

## Submit the Lab

### Time Spent

Create `time.txt` with a single integer: hours spent. Then `git add` and `git commit`.

### Answers

If this lab has questions, write answers in `answers-*.txt`, then `git add` and `git commit`.

### Submit

Submissions are via Gradescope. You need an MIT Gradescope account; see Piazza for class entry code. If needed, use [this help link](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code).

Run `make zipball` to generate `lab.zip`, then upload it.

If there are uncommitted/untracked files, you may see:

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

Make sure required files are tracked (not `??`). Use `git add {filename}`.

- Run `make grade` before submission.
- Commit modified source before `make zipball`.
- Gradescope lab grade is the final lab grade.

---

*Questions or comments regarding 6.1810? Send e-mail to the course staff at 61810-staff@lists.csail.mit.edu.*
