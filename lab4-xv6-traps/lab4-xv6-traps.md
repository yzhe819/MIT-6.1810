# Lab 4: Traps

[English](./lab4-xv6-traps.md) · [中文](./lab4-xv6-traps-zh.md)

This lab explores how system calls are implemented using traps. You will first do a warm-up exercise with stacks and then implement an example of user-level trap handling.

Before you start coding, read Chapter 4 of the [xv6 book](xv6/book-riscv-rev5.pdf), and related source files:

- `kernel/trampoline.S`: the assembly involved in changing from user space to kernel space and back
- `kernel/trap.c`: code handling all interrupts

To start the lab, switch to the `traps` branch:

```bash
$ git fetch
$ git checkout traps
$ make clean
```

---

## RISC-V Assembly (easy)

It is important to understand some RISC-V assembly, which you were exposed to in 6.1910 (6.004). There is a file `user/call.c` in your xv6 repo. `make fs.img` compiles it and also produces a readable assembly version of the program in `user/call.asm`.

Read the code in `call.asm` for the functions `g`, `f`, and `main`. The instruction manual for RISC-V is on the [reference page](reference.html). Answer the following questions in `answers-traps.txt`:

1. Which registers contain arguments to functions? For example, which register holds `13` in `main`'s call to `printf`?
2. Where is the call to function `f` in the assembly code for `main`? Where is the call to `g`? (Hint: the compiler may inline functions.)
3. At what address is the function `printf` located?
4. What value is in register `ra` just after the `jalr` to `printf` in `main`?
5. In the following code, what is going to be printed after `y=`? (Note: the answer is not a specific value.) Why does this happen?

```c
printf("x=%d y=%d", 3);
```

---

## Backtrace (moderate)

For debugging, it is often useful to have a backtrace: a list of function calls on the stack above the point where an error occurred. To help with backtraces, the compiler generates machine code that maintains a stack frame for each function in the current call chain. Each stack frame contains the return address and a "frame pointer" to the caller's stack frame. Register `s0` contains a pointer to the current stack frame (it actually points to the address of the saved return address on the stack plus 8). Your backtrace should use frame pointers to walk up the stack and print the saved return address in each stack frame.

Implement a `backtrace()` function in `kernel/printf.c`. Insert a call to this function in `sys_pause`, then run `bttest`, which calls `sys_pause`. Your output should be a list of return addresses like this (numbers will likely differ):

```
backtrace:
0x0000000080002cda
0x0000000080002bb6
0x0000000080002898
```

After `bttest`, exit QEMU. In a terminal window, run `addr2line -e kernel/kernel` (or `riscv64-unknown-elf-addr2line -e kernel/kernel`) and paste addresses from your backtrace:

```bash
$ addr2line -e kernel/kernel
0x0000000080002de2
0x0000000080002f4a
0x0000000080002bfc
Ctrl-D
```

You should see output like:

```
kernel/sysproc.c:74
kernel/syscall.c:224
kernel/trap.c:85
```

Some hints:

- Add the prototype for `backtrace()` to `kernel/defs.h` so `sys_pause` can call it.
- GCC stores the frame pointer of the currently executing function in register `s0`. In the section marked by `#ifndef __ASSEMBLER__ ... #endif`, add this function to `kernel/riscv.h`:

```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x));
  return x;
}
```

Then call this function in `backtrace` to read the current frame pointer. `r_fp()` uses [in-line assembly](https://gcc.gnu.org/onlinedocs/gcc/Using-Assembly-Language-with-C.html) to read `s0`.
- These [lecture notes](https://pdos.csail.mit.edu/6.1810/2023/lec/l-riscv.txt) include a diagram of stack frame layout. Note that the return address lives at fixed offset `-8` from the frame pointer of a stack frame, and the saved frame pointer lives at fixed offset `-16`.
- Your `backtrace()` needs a way to detect the last stack frame and stop. A useful fact is that each kernel stack is one page-aligned page, so all stack frames for a given stack are on the same page. You can use `PGROUNDDOWN(fp)` (see `kernel/riscv.h`) to identify the page referred to by a frame pointer.

Once your backtrace works, call it from `panic` in `kernel/printf.c` so you can see the kernel backtrace when it panics.

---

## Alarm (hard)

In this exercise, you will add a feature to xv6 that periodically alerts a process as it consumes CPU time. This can help compute-bound processes limit CPU usage, or perform periodic actions while computing. More generally, you will implement a primitive form of user-level interrupt/fault handlers; a similar mechanism could, for example, handle page faults in an application. Your solution is correct if it passes `alarmtest` and `usertests -q`.

You should add a new `sigalarm(interval, handler)` system call. If an application calls `sigalarm(n, fn)`, then after every `n` "ticks" of CPU time consumed by the program, the kernel should cause application function `fn` to be called. When `fn` returns, the application should resume where it left off. A tick is a unit of time in xv6 defined by the hardware timer interrupt frequency. If an application calls `sigalarm(0, 0)`, the kernel should stop generating periodic alarm calls.

You will find `user/alarmtest.c` in your xv6 repository. Add it to the Makefile. It won't compile until you've added `sigalarm` and `sigreturn` system calls (see below).

`alarmtest` calls `sigalarm(2, periodic)` in `test0` to ask the kernel to force a call to `periodic()` every 2 ticks, then spins for a while. You can inspect `user/alarmtest.asm` for debugging. Your solution is correct when `alarmtest` and `usertests -q` produce output like:

```bash
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
test3 start
test3 passed
$ usertests -q
...
ALL TESTS PASSED
$
```

When finished, your solution may be only a few lines, but getting it right can be tricky. The grader uses the original `alarmtest.c` from the repository. You may modify `alarmtest.c` for debugging, but make sure the original version passes all tests.

### test0: invoke handler

Start by modifying the kernel to jump to the alarm handler in user space, so `test0` prints `alarm!`. For now, don't worry about what happens after that output; it is OK if the program crashes after printing `alarm!`.

Hints:

- Modify the Makefile so `alarmtest.c` is compiled as an xv6 user program.
- Add these declarations to `user/user.h`:

```c
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```

- Update `user/usys.pl` (which generates `user/usys.S`), `kernel/syscall.h`, and `kernel/syscall.c` to allow `alarmtest` to invoke `sigalarm` and `sigreturn`.
- For now, your `sys_sigreturn` can simply return zero.
- `sys_sigalarm()` should store the alarm interval and handler function pointer in new fields in the `proc` structure (`kernel/proc.h`).
- Track how many ticks have passed since the last alarm handler call (or ticks left until next call) in another new field in `struct proc`. You can initialize these fields in `allocproc()` in `proc.c`.
- Every tick, hardware timer interrupt handling goes through `usertrap()` in `kernel/trap.c`.
- Only manipulate alarm ticks on timer interrupts, e.g.:

```c
if (which_dev == 2) ...
```

- Only invoke the alarm function if the process has an active timer. Note that the address of the user's alarm function may be 0 (e.g., in `user/alarmtest.asm`, `periodic` is at address 0).
- Modify `usertrap()` so when alarm interval expires, user code starts executing the handler function. When a RISC-V trap returns to user space, what determines the instruction address where user code resumes?
- GDB inspection is easier with a single CPU:

```bash
make CPUS=1 qemu-gdb
```

- You've succeeded when `alarmtest` prints `alarm!`.

### test1/test2()/test3(): resume interrupted code

It is likely that `alarmtest` crashes in `test0` or `test1` after printing `alarm!`, eventually prints `test1 failed`, or exits without printing `test1 passed`. To fix this, ensure that when alarm handler returns, control resumes at the instruction where the timer interrupt originally interrupted the user program. You must restore register contents to the values at interrupt time, so the user program continues undisturbed. Finally, re-arm the alarm counter each time it fires so the handler is called periodically.

As a starting point, the lab requires user alarm handlers to call `sigreturn` when they finish. See `periodic` in `alarmtest.c`. This means `usertrap` and `sys_sigreturn` can cooperate to resume correctly after handling an alarm.

Some hints:

- You must save and restore registers. Which registers are needed for correct resume? (Hint: many.)
- Have `usertrap` save enough state in `struct proc` when timer fires so `sigreturn` can return correctly to interrupted user code.
- Prevent re-entrant handler calls: if handler has not returned, kernel should not call it again. `test2` checks this.
- Make sure to restore `a0`. `sigreturn` is a system call, and its return value is stored in `a0`.

After passing `test0`, `test1`, `test2`, and `test3`, run `usertests -q` to ensure no other kernel behavior was broken.

---

## Submit the Lab

### Time Spent

Create `time.txt` containing a single integer: the number of hours spent on this lab. `git add` and `git commit` the file.

### Answers

If this lab has questions, write answers in `answers-*.txt`. `git add` and `git commit` these files.

### Submit

Assignment submission is through Gradescope. You need an MIT Gradescope account. See Piazza for the class entry code. Use [this link](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code) if needed.

When ready, run `make zipball` to generate `lab.zip`, then upload this zip to the corresponding Gradescope assignment.

If you run `make zipball` with uncommitted changes or untracked files, you may see output like:

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

Inspect those lines and ensure all files required for your solution are tracked (i.e., not listed with `??`). You can track a new file via `git add {filename}`.

- Run `make grade` to ensure all tests pass. Gradescope autograder uses the same grading program.
- Commit modified source code before running `make zipball`.
- You can inspect submission status and download submitted code on Gradescope. The Gradescope lab grade is your final lab grade.

---

## Optional Challenge Exercises

- Print function names and line numbers in `backtrace()` instead of numeric addresses.

---

*Questions or comments regarding 6.1810? Send e-mail to the course staff at 61810-staff@lists.csail.mit.edu.*
