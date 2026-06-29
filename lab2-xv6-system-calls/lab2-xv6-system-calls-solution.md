# Lab 2 Write-up

**Overview:** Understand the system call mechanism and the transition between user mode and kernel mode.

**Difficulty:** `Using GDB` (Easy) · `Sandbox a command` (Moderate) · `Sandbox with allowed pathnames` (Easy) · `Attack xv6` (Moderate)

## Using GDB (Easy)

The lab begins by opening two terminal windows simultaneously — one running `make qemu-gdb` and the other running `gdb` — to verify that GDB is set up correctly. Since this lab was done on macOS, the setup differs slightly from the standard course environment. A RISC-V compatible version of GDB needs to be installed separately:

```bash
brew install riscv64-elf-gdb
riscv64-elf-gdb --version
riscv64-elf-gdb    # use this in place of the standard gdb command
```

Once both windows are running, the setup looks like this (first screenshot: `make qemu-gdb`, second: `riscv64-elf-gdb`):

![2026-06-25 23.51.11](./2026-06-25%2023.51.11.png)

![2026-06-25 21.39.14](./2026-06-25%2021.39.14.png)

Following the lab instructions, enter the following commands in the GDB window:

```
(gdb) b syscall
Breakpoint 1 at 0x80002142: file kernel/syscall.c, line 243.
(gdb) c
Continuing.
[Switching to Thread 1.2]

Thread 2 hit Breakpoint 1, syscall () at kernel/syscall.c:243
243     {
(gdb) layout src
(gdb) backtrace
```

![2026-06-25 21.40.49](./2026-06-25%2021.40.49.png)

After running `layout src` and `backtrace`, the output looks like this:

![2026-06-25 21.41.15](./2026-06-25%2021.41.15.png)

![2026-06-25 21.41.47](./2026-06-25%2021.41.47.png)

`layout src` shows the source code at the current execution point, and `backtrace` prints the full call stack.

---

### Q1: Looking at the backtrace output, which function called syscall?

From the `backtrace` output, `usertrap()` in `kernel/trap.c` at line 68 called `syscall()`. Looking at the source confirms this — `usertrap()` is the entry point responsible for dispatching system calls in the user trap handling flow.

---

Step through with `n` to advance line by line. After executing `struct proc *p = myproc();`, enter `p /x *p` in GDB to print the current process's `proc` struct (defined in `kernel/proc.h`) in hexadecimal.

![2026-06-25 21.44.34](./2026-06-25%2021.44.34.png)

![2026-06-25 21.45.31](./2026-06-25%2021.45.31.png)

---

### Q2: What is the value of p->trapframe->a7, and what does it represent? (Hint: see user/init.c and user/init.asm)

Running `p /x p->trapframe->a7` prints `0xf`, which is 15 in decimal.

Looking at `user/init.asm` — the first user program xv6 runs at boot:

```asm
int
main(void)
{
   0:	1101                	addi	sp,sp,-32
   2:	ec06                	sd	ra,24(sp)
   4:	e822                	sd	s0,16(sp)
   6:	e426                	sd	s1,8(sp)
   8:	e04a                	sd	s2,0(sp)
   a:	1000                	addi	s0,sp,32
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
   c:	4589                	li	a1,2
   e:	00001517          	auipc	a0,0x1
  12:	96250513          	addi	a0,a0,-1694 # 970 <malloc+0xf8>
  16:	39e000ef          	jal	3b4 <open>
  1a:	04054563          	bltz	a0,64 <main+0x64>
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
```

Under the RISC-V system call convention, the syscall number is placed in register `a7` before issuing an `ecall`. The first system call made by `init` is `open("console", O_RDWR)`. Looking up `kernel/syscall.h` confirms `#define SYS_open 15`, which matches the observed value exactly.

The full call chain:

```
init starts
    └─→ open("console", O_RDWR)
            └─→ a7 = 15 (SYS_open)
            └─→ ecall triggers trap → kernel/syscall.c syscall()
            └─→ num = p->trapframe->a7  // = 0xf = 15
            └─→ syscalls[15]()          // i.e. sys_open()
```

---

Next, run `p /x $sstatus` in GDB to print the full contents of the Supervisor Status Register:

![2026-06-25 23.52.06](./2026-06-25%2023.52.06.png)

The result is `$sstatus = 0x200000022`.

---

### Q3: What CPU mode was the processor in before the trap occurred?

The RISC-V privileged ISA manual (§4.1.1) states:

> "The SPP bit indicates the privilege level at which a hart was executing before entering supervisor mode. When a trap is taken, SPP is set to 0 if the trap originated from user mode, or 1 otherwise."

Breaking down `0x200000022`:

```
0x200000022

Expanded in binary:
  2    0    0    0    0    0    0    2    2
0010 0000 0000 0000 0000 0000 0000 0010 0010

bit 1  (SIE)  = 1  → Supervisor interrupt enable
bit 5  (SPIE) = 1  → Interrupts were enabled before the trap
bit 8  (SPP)  = 0  → Privilege level before the trap
bit 33 (SD)   = 1  → Floating-point/vector state is dirty (minor)
```

The key field is **SPP (bit 8) = 0**.

SPP (Supervisor Previous Privilege) is set automatically by hardware when a trap occurs, recording the privilege level the CPU was at before entering Supervisor mode:

- `SPP = 0` → came from **U-mode (user mode)**
- `SPP = 1` → came from **S-mode (kernel mode)**

**Conclusion:** the CPU was in user mode before the trap. The `init` process, as the first user program launched by xv6, called `open()` from user mode, which triggered an `ecall` and dropped into Supervisor mode — exactly as expected.

---

With this stage of debugging complete, close both windows and modify `kernel/syscall.c`, changing:

```c
num = p->trapframe->a7;
```

to:

```c
num = *(int *)0;
```

The modified `syscall()` function looks like this:

```c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = *(int *)0;    // intentional illegal memory access
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

Running `make qemu` now causes the kernel to panic as expected, with the error: `scause=0xd sepc=0x80001d08 stval=0x0`. Look up the value of `sepc` (`0x80001d08`) in `kernel/kernel.asm` to find the corresponding compiled instruction.

![2026-06-25 22.07.57](./2026-06-25%2022.07.57.png)

---

### Q4: What is the assembly instruction that caused the kernel panic? Which register holds the variable num?

![2026-06-25 22.11.29](./2026-06-25%2022.11.29.png)

The instruction at address `0x80001d08` in `kernel/kernel.asm` is:

```asm
80001d08:   00002683    lw  a3, 0(zero)  # 0 <_entry-0x80000000>
```

This instruction attempts to load a word from virtual address `0x0` (register `zero` plus offset 0) into register `a3`. Therefore, **the variable `num` maps to register `a3`**.

Setting a breakpoint at that address with `b *0x80001d08` and using `layout asm` to inspect the assembly confirms this:

![2026-06-25 22.17.30](./2026-06-25%2022.17.30.png)

![2026-06-25 22.17.43](./2026-06-25%2022.17.43.png)

Continuing execution, the program stops at the breakpoint as expected — the instruction is attempting to load the contents of address `0x0` into register `a3`:

![2026-06-25 22.28.05](./2026-06-25%2022.28.05.png)

---

### Q5: Why did the kernel crash? Hint: look at figure 3-3 in the textbook. Is address 0 mapped in the kernel address space? Does the value of scause confirm this?

From textbook figure 3-3, xv6's kernel address space layout is as follows:

```
0xFFFFFFFF         ┌─────────────────┐
                   │   DEVSPACE      │  (memory-mapped devices)
0x80100000         ├─────────────────┤
                   │   kernel text   │  (kernel code, starting at 0x80000000)
                   │   kernel data   │
                   │   free memory   │
0x80000000         ├─────────────────┤  ← kernel address space starts here
                   │                 │
                   │   (unmapped)    │  ← no page table entries here
                   │                 │
0x00000000         └─────────────────┘  ← address 0x0 is here
```

Address `0x0` falls in the unmapped region of the kernel page table. When the CPU executes `lw a3, 0(zero)`, the MMU finds no valid mapping for that virtual address and raises an exception.

According to the RISC-V privileged spec, `scause = 0xd` (decimal 13) corresponds to a **load page fault** — exactly what happens when a load instruction targets a virtual address with no valid page table entry.

All three registers tell a consistent story:

| Register | Value | Meaning |
|----------|-------|---------|
| `scause` | `0xd` | Load page fault (no mapping for the address) |
| `sepc` | `0x80001d08` | Address of the faulting instruction |
| `stval` | `0x0` | The illegal address that was accessed |

Since a page fault triggered inside the kernel itself is unrecoverable (unlike a user process, which can simply be killed), the kernel has no choice but to `panic` and halt.

---

### Q6: Which process was running when the kernel panicked, and what was its pid?

Running the following in GDB prints the current process info directly:

```gdb
(gdb) p p->name
(gdb) p p->pid
```

Results:

- **Process name:** `init`
- **pid:** `1`

![2026-06-25 22.28.27](./2026-06-25%2022.28.27.png)

`init` is the first user process created at boot, and its pid is always 1. The panic occurred while this process was executing its very first system call — `open("console", O_RDWR)` — which is consistent with everything observed above.

## Sandbox a command (moderate)

Following the lab hints, start by adding `$U/_sandbox` to `UPROGS` in the `Makefile`, adding the function prototype `int interpose(int, char*);` to `user/user.h`, and registering the stub in `user/usys.pl`. That covers everything on the user side.

> The Makefile invokes the perl script `user/usys.pl`, which produces `user/usys.S`, the actual system call stubs, which use the RISC-V `ecall` instruction to transition to the kernel.

On the kernel side, assign a new syscall number in `kernel/syscall.h` with `#define SYS_interpose 22`, then add the function declaration in `kernel/syscall.c` and register the mapping in the syscalls array.

`sys_interpose()` is implemented in `kernel/sysproc.c`. The first task is reading the mask argument from the user. Looking through the file, `sys_pause()` from Lab 1 serves as a handy reference:

```c
uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
```

Following the same pattern, `argint(0, &mask)` reads the first integer argument. A quick `printf` confirms the value is coming through correctly:

```c
uint64
sys_interpose(void)
{
  int n;
  argint(0, &n);
  printf("mask: %d\n", n);
  return 0;
}
```

> recording the mask argument in a new field in the proc structure (see kernel/proc.h).

Following the hint, add `int mask;` to the `proc` struct. Then update `sys_interpose()` to persist the value by writing it into the current process via `myproc()`:

```c
uint64
sys_interpose(void)
{
  int mask;
  argint(0, &mask);
  printf("mask: %d\n", mask);
  myproc()->mask = mask;
  return 0;
}
```

To make child processes inherit the parent's restrictions, add `np->mask = p->mask` in `kfork()` in `kernel/proc.c`:

```c
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // also copy the mask argument from the parent process
  np->mask = p->mask;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  ···
```

Finally, enforce the restriction in the `syscall()` function in `kernel/syscall.c`. If the current syscall number has its corresponding bit set in the mask, reject it immediately. Since only one syscall number is handled per invocation, a single bit-shift check is enough:

```c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    if ((p->mask >> num) & 1) {
      p->trapframe->a0 = -1;
      return;
    }
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

---

## Sandbox with allowed pathnames (Easy)

> If `open` or `exec` is masked but the pathname matches the allowed pathname, these system calls should still be allowed.

The wording here is a bit convoluted. In plain terms: add a second argument to `interpose` specifying an allowed path. Even if `open` or `exec` is blocked by the mask, the call should go through if the path matches.

Add a `char path[MAXPATH]` field to the `proc` struct. In `sys_interpose()`, read the second argument with `argstr(1, path, MAXPATH)` and store it using `safestrcpy()` — direct pointer assignment won't work here:

```c
uint64
sys_interpose(void)
{
  int mask;
  char path[MAXPATH];

  argint(0, &mask);
  argstr(1, path, MAXPATH);

  // printf("mask: %d\n", mask);
  // printf("path: %s\n", path);

  myproc()->mask = mask;
  safestrcpy(myproc()->path, path, sizeof(path));

  return 0;
}
```

The path also needs to be inherited on fork, so add `safestrcpy(np->path, p->path, sizeof(p->path))` alongside the mask copy in `kfork()`.

When `open` or `exec` is masked, they cannot be rejected outright in `syscall()` — the path argument isn't known at that point. Instead, let them fall through to their own handlers, which can then do the path check. Add an exception in `syscall()` for these two:

```c
if ((p->mask >> num) & 1) {
  if (num != SYS_open && num != SYS_exec) {
    p->trapframe->a0 = -1;
    return;
  }
}
```

The path check itself goes in `sys_open()` and `sys_exec()` in `kernel/sysfile.c`. For `sys_open()`, insert the check after `argstr()` reads the path but before `begin_op()`:

```c
struct proc *p = myproc();
if ((p->mask >> SYS_open) & 1) {
  if (strncmp(path, p->path, MAXPATH) != 0) {
    return -1;
  }
}
```

`sys_exec()` follows the same pattern, with `SYS_open` replaced by `SYS_exec`:

```c
struct proc *p = myproc();
if ((p->mask >> SYS_exec) & 1) {
  if (strncmp(path, p->path, MAXPATH) != 0) {
    return -1;
  }
}
```

The logic is a bit layered — the mask determines whether a path check is needed, and the path check determines whether to allow the call. It helps to think it through carefully before writing any code.

## Attack xv6 (Moderate)

The lab description explains that the logic for zeroing newly allocated pages has been removed, as has the code that fills freed pages with garbage data. The result is that newly allocated memory retains whatever was left behind by its previous owner. If that previous owner wrote something sensitive, any program that gets the same physical page can simply read it.

The goal is to implement `user/attack.c` to recover the secret that `secret.c` wrote to memory.

The key to locating the secret lies in `secret.c`'s write pattern:

```c
strcpy(data, "This may help.");
strcpy(data + 16, argv[1]);
```

`"This may help."` is a fixed marker string, and the secret sits exactly 16 bytes after it — not unlike using an HTML id or CSS class name to anchor a web scraper. Scanning the allocated memory for that marker and printing what follows is all that's needed. `printf`'s `%s` stops at `'\0'` automatically, so no manual boundary handling is required:

```c
#include "kernel/types.h"
#include "user/user.h"

#define DATASIZE (8 * 4096)

int
main(void)
{
  char *mem = sbrk(DATASIZE);
  const char *hint = "This may help.";

  for (int i = 0; i < DATASIZE - 16; i++) {
    if (!strcmp(mem + i, hint)) {
      printf("%s\n", mem + i + 16);
      break;
    }
  }

  exit(0);
}
```

The test only guarantees that the secret contains alphanumeric characters, but since `%s` handles termination automatically, no additional filtering is needed.

## Full Lab 2 Test

Once all exercises are complete, run the following to grade the entire lab:

```sh
./grade-lab-syscall
```

![2026-06-29 21.12.39](./2026-06-29%2021.12.39.png)
