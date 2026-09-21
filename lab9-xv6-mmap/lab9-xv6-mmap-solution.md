# Lab 9 Report (AI)

**Objective Summary:** Implement file-backed lazy memory mapping (`mmap`/`munmap`) in xv6, including VMA management, page fault-triggered allocation and loading, shared mapping write-back, and mapping handling under `fork`/`exit`.

**Experiment Difficulty:** `mmap/munmap` (hard) · `VMA + Page Fault Handling Path` (hard)

## `mmap` (hard)

This article is an implementation note for [MIT 6.1810 Lab: mmap](https://pdos.csail.mit.edu/6.1810/2025/labs/mmap.html). There is only one task in this lab, but it's a classic one—from MIT 6.828 to MIT 6.S081, and now 6.1810, `mmap`/`munmap` has always been the finale lab task.

This lab requires us to add two system calls, `mmap` and `munmap`, in xv6, with the focus on supporting memory-mapped files. In short, it allows a file to be "mapped" into the address space of a process, so the process can directly read and write this address range as if it were memory. Under the hood, the kernel is responsible for transferring data between memory and disk files, without the user needing to explicitly call `read`/`write`.

There are a few points that require special attention:

- **Lazy allocation**: When `mmap` is called, physical memory is not actually allocated, nor is the file content read. It only "reserves" an area in the process's address space. The actual physical page allocation and file content loading occur after the first access to this memory triggers a page fault—which means "load on demand."
- **Recycling and write-back in `munmap`**: `munmap` is responsible for revoking the mapping and freeing the corresponding physical memory; if this mapping is `MAP_SHARED` and the memory content has been modified, it also needs to write this part of the content back to the original file on the disk.
- **Mapping relationships during `fork`/`exit`**: When a child process is created via `fork`, it should inherit the mapping relationships of the parent process. When a process calls `exit`, it also needs to perform a write-back and cleanup equivalent to calling `munmap` on all its `MAP_SHARED` mappings.

### System Call Registration

The first step is to register the two new system calls, `mmap` and `munmap`, in xv6. The steps are exactly the same as adding system calls in previous labs—configure `kernel/syscall.h` (add system call numbers), `kernel/syscall.c` (register to the call table), `user/user.h`, and `user/usys.pl` (export to user space), and finally add `mmaptest` to the `UPROGS` in the `Makefile`. We won't implement the specific logic in this step; just let `sys_mmap`/`sys_munmap` directly return `-1` so that `user/mmaptest.c` can compile and run. At this point, running the test will show that it gets stuck on the first `mmap` call.

### VMA Data Structure

Next, we need to define a data structure `vma` (virtual memory area). Since a process might map multiple files simultaneously, we need an array to record which areas are currently mapped and the detailed information for each area.

First, define the maximum number of `vma`s each process can have in `kernel/param.h`:

> Each process can maintain up to 16 mapped areas simultaneously—this number is also the suggested size in the hints, which is sufficient for `mmaptest`.

```h
#define MAXPATH      128   // maximum file path name
#define USERSTACK    1     // user stack pages
#define NVMA         16
```

Then, define the `vma` structure itself in `kernel/proc.h`:

```h
// vma state
struct vma {
  int valid; // this vma is be using or not
  uint64 addr; // the va start of this mapping
  size_t len;
  int prot; // the prot_read / prot_write premission record
  int flags; // use to record map_shared or map_private
  struct file *file;
  off_t offset; // usually is 0, just the placeholder here
};
```

Then add the `vma` structure array to the process and attach `vma` to `struct proc`:

```h
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  // add vma support array
  struct vma vmas[NVMA];
  // the top address
  uint64 mmapend;
};
```

The newly added `mmapend` in `struct proc` records the available allocation address. The memory space starting from this position is available.

> `proc.h` uses `size_t`/`off_t`, but does not include the header files defining them.
> We need to include the header at the beginning - `#include "defs.h"`.

### Initialization and Cleanup in `allocproc` / `freeproc`

With the data structures in place, we also need to initialize and clean up these new fields where processes are created and destroyed. This corresponds to `allocproc` and `freeproc` in `kernel/proc.c`.

```c
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  p->mmapend = TRAPFRAME;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  // init all the vma space
  for(int i = 0; i < NVMA; i++) {
    memset(&p->vmas[i], 0, sizeof(p->vmas[i]));
  }

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  for(int i = 0; i < NVMA; i++) {
    memset(&p->vmas[i], 0, sizeof(p->vmas[i]));
  }
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  p->mmapend = 0;
}
```

There are two new additions in `allocproc`: first, `p->mmapend` is initialized to `TRAPFRAME` as the starting position for this process's `mmap` area; second, the `vmas` array is reset at the end of the function to ensure that the `vma` slots in each newly allocated process are in a clean state. `freeproc` similarly handles resetting.

### `sys_mmap`

Now we can officially implement the function logic. I moved `sys_mmap`/`sys_munmap` from `kernel/sysproc.c` to `kernel/sysfile.c`. There are two reasons for this: first, this part is more closely related to files; second, `argfd()` is declared as `static` in `kernel/sysfile.c`, making it visible only within that file. Since `sys_mmap` needs to get a `struct file *` through the user-passed `fd` (i.e., it needs to reuse `argfd`), it must be placed in the same compilation unit (otherwise it will cause a linkage error).

The implementation is as follows: (read user input and find a suitable slot in the process to record it)

```c
uint64
sys_mmap(void)
{
  uint64 addr;
  int len, prot, flags, offset;
  struct file* f;

  argaddr(0, &addr);
  argint(1, &len);
  argint(2, &prot);
  argint(3, &flags);
  if(argfd(4, 0, &f) < 0)
    return -1;
  argint(5, &offset);

  // get the current proc and setup the vma
  struct proc* p = myproc();

  // check read/write prot is match with the premission
  if((prot & PROT_READ) && !f->readable)
    return -1;

  if((prot & PROT_WRITE) && (flags & MAP_SHARED) && !f->writable)
    return -1;

  for(int i = 0; i < NVMA; i++) {
    if(p->vmas[i].valid == 0) {
      p->vmas[i].valid = 1;
      // set the give memory space from begin va
      uint64 end = PGROUNDUP(len);
      p->mmapend -= end;
      uint64 begin = p->mmapend;
      p->vmas[i].addr = begin;
      p->vmas[i].len = len;
      p->vmas[i].prot = prot;
      p->vmas[i].flags = flags;
      p->vmas[i].offset = offset;
      filedup(f);
      p->vmas[i].file = f;
      return p->vmas[i].addr;
    }
  }

  // cannot find the vma slot for this proc
  return -1;
}

uint64
sys_munmap(void)
{
  return -1;
}
```

If you check my code, you'll find that my initial implementation was to allocate `mmap` addresses starting from `PGROUNDUP(p->sz)` and advance `p->sz`.
However, doing this exposed a bug in the "test munmap prevents access" test: after a child process unmaps (`munmap`) one page and accesses the same address again, since `munmap` did not rollback `p->sz`, this address was still less than `p->sz`. This caused `vmfault` to misidentify it as a valid heap lazy allocation and allocate a page out of thin air, returning success instead of the expected "illegal access, kill process."

The final chosen approach is to have `mmap` use an address range completely independent of `p->sz` (using `PGROUNDUP` to cut downwards from the top of `TRAPFRAME`). This physically isolates the heap and the `mmap` areas. Thus, re-accessing after `munmap` will no longer be misidentified as heap access, entirely avoiding this issue.

### `vmfault`

Similar to the previous lab, actual allocation happens after the first access to this memory triggers a page fault (lazy loading).

The original lab code already identifies and handles page faults caused by `mmap` in `usertrap`. Below is the decision entry point:

```c
} else if((r_scause() == 15 || r_scause() == 13) &&
          vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
  // page fault on lazily-allocated page
} else {
```

The actual processing logic will be in `vmfault(pagetable_t pagetable, uint64 va, int read)`. We go to `kernel/vm.c` to modify it:

```c
// allocate and map user memory if process is referencing a page
// that was lazily allocated in sys_sbrk().
// returns 0 if va is invalid or already mapped, or if
// out of physical memory, and physical address if successful.
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc* p = myproc();

  va = PGROUNDDOWN(va);

  if(va >= TRAPFRAME)
    return 0;
  
  if(ismapped(pagetable, va)) {
    return 0;
  }

  for(int i = 0; i < NVMA; i++) {
    // check the using vma
    if(p->vmas[i].valid == 1) {
      // this the correct vma
      if(p->vmas[i].addr <= va && va < (p->vmas[i].addr + p->vmas[i].len)) {
        uint64 end = p->vmas[i].addr + p->vmas[i].len;
        int size = PGSIZE;
        if(va + PGSIZE > end)
          size = end - va;

        // init mem
        mem = (uint64)kalloc();
        if(mem == 0)
          return 0;
        memset((void*)mem, 0, PGSIZE);

        // read the file data into the mem
        struct inode* ip = p->vmas[i].file->ip;
        ilock(ip);
        readi(ip, 0, mem, va - p->vmas[i].addr + p->vmas[i].offset, size);
        iunlock(ip);

        // update the prot for this page
        int prot = p->vmas[i].prot;
        int perm = PTE_U;
        if(prot & PROT_READ)
          perm |= PTE_R;
        if(prot & PROT_WRITE)
          perm |= PTE_W;

        if(mappages(p->pagetable, va, size, mem, perm) != 0) {
          kfree((void*)mem);
          return 0;
        }

        return mem;
      }
    }
  }

  // not belongs to any vma -> check the stack allocation
  if(va >= p->sz)
    return 0;

  mem = (uint64)kalloc();
  if(mem == 0)
    return 0;
  memset((void*)mem, 0, PGSIZE);
  if(mappages(p->pagetable, va, PGSIZE, mem, PTE_W | PTE_U | PTE_R) != 0) {
    kfree((void*)mem);
    return 0;
  }
  return mem;
}
```

This line was added in `vmfault`: `if(va >= TRAPFRAME) return 0;`

Because the test cases intentionally test illegal addresses exceeding `MAXVA`, without this boundary check, the address would be passed directly to `ismapped()` → `walk()`, and `walk()` will directly `panic` on addresses exceeding `MAXVA`. Since this cuts downward from the top of the `TRAPFRAME`, it's not entirely equivalent to the original "check `va >= p->sz`" position (which would block valid high-address `mmap` accesses).

> Because `p->vmas[i].file->ip` is used in `vmfault`, but `vm.c` does not have `#include "file.h"`.
> `PROT_READ`/`PROT_WRITE` undeclared: requires `#include "fcntl.h"`.

> Therefore, pay special attention to package imports here in vm.

I added these includes at the beginning of the file (and the order must not be scrambled):

```c
#include "defs.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "file.h"
```

### `sys_munmap`

Next, we can add the memory unmap logic:

```c
uint64
sys_munmap(void)
{
  uint64 addr;

  int len;
  int i = 0;

  argaddr(0, &addr);
  argint(1, &len);

  struct proc* p = myproc();

  for(i = 0; i < NVMA; i++) {
    if(p->vmas[i].valid == 1) {
      if(p->vmas[i].addr <= addr && addr < (p->vmas[i].addr + p->vmas[i].len)) {
        break;
      }
    }
  }

  // not found the related vma, return
  if(i == NVMA) {
    return -1;
  }

  struct vma* vma = &p->vmas[i];

  vmaunmap(p, vma, addr, len);

  if(addr == vma->addr && len == vma->len) {
    fileclose(vma->file);
    vma->valid = 0;
  } else if(addr == vma->addr) {
    vma->addr += len;
    vma->offset += len;
    vma->len -= len;
  } else if((addr + len) == (vma->addr + vma->len)) {
    vma->len -= len;
  }

  return 0;
}

void
vmaunmap(struct proc* p, struct vma* vma, uint64 addr, uint64 len)
{
  uint64 pa;
  for(uint64 va = addr; va < addr + len; va += PGSIZE) {
    if((pa = walkaddr(p->pagetable, va)) != 0) {
      if(vma->flags & MAP_SHARED) {
        uint fileoff = (va - vma->addr) + vma->offset;
        struct inode* ip = vma->file->ip;
        begin_op();
        ilock(ip);
        uint size;
        if(fileoff >= ip->size) {
          size = 0;
        } else if(fileoff + PGSIZE > ip->size) {
          size = ip->size - fileoff;
        } else {
          size = PGSIZE;
        }
        if(size > 0) {
          writei(ip, 0, pa, fileoff, size);
        }
        iunlock(ip);
        end_op();
      }
      uvmunmap(p->pagetable, va, 1, 1);
    } else {
      // not be mapped, skip this page
      continue;
    }
  }
}
```

`vmaunmap` (writing back to the file) should use `ip->size` instead of `vma->len`. `vma->len` is the mapping length declared by `mmap`, whereas `ip->size` is the actual size of the file, and the two are often inconsistent (e.g., `mmap(..., PGSIZE*3, ...)` mapping a file with only 1.5 pages of content). If `vma->len` is used to determine "whether it is the last page," it will cause `writei` to write invalid bytes back.

> "dirty read #2" in "test mmap dirty" specifically tests this.

### `exit` and Writing to Real Files

First, update the `kernel/defs.h` file, as the related data structures and methods will be needed later:

```c
// add vma handle function
struct proc;
struct vma;
void vmaunmap(struct proc *p, struct vma *vma, uint64 addr, uint64 len);
```

> Note: You cannot directly `#include "proc.h"` here, as it will cause circular header file inclusion.
> Changing it to just a forward declaration `struct proc;` / `struct vma;` (since the function declaration only uses pointer types and doesn't need the complete definition) solves the problem.

Modify `exit` so that when a process exits, it writes `MAP_SHARED` mappings back to the file.

```c
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
kexit(int status)
{
  struct proc* p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++) {
    if(p->ofile[fd]) {
      struct file* f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  for(int i = 0; i < NVMA; i++) {
    if(p->vmas[i].valid == 1) {
      struct file* f = p->vmas[i].file;
      vmaunmap(p, &p->vmas[i], p->vmas[i].addr, p->vmas[i].len);
      fileclose(f);
      p->vmas[i].file = 0;
      p->vmas[i].valid = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}
```

### Final Step: Updating `fork`

Modify `fork` so that the child process inherits the parent process's mappings.

```c
// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
kfork(void)
{
  int i, pid;
  struct proc* np;
  struct proc* p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0) {
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  np->mmapend = p->mmapend;

  // copy the existing vmas list and invoke the fileup
  for(i = 0; i < NVMA; i++) {
    if(p->vmas[i].valid == 1) {
      np->vmas[i] = p->vmas[i];
      filedup(np->vmas[i].file);
    }
  }

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}
```

## Lab 9 Comprehensive Test

After completing all exercises, run the following command to perform a complete test of Lab 9 to verify the correctness of each implemented function:

```sh
./grade-lab-mmap
```

![2026-07-19 20.52.21](./2026-07-19%2020.52.21.png)
---
