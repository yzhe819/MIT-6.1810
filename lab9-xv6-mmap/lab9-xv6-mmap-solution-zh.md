# Lab 9 实验报告

**目标简述：** 在 xv6 中实现基于文件的懒加载内存映射（`mmap`/`munmap`），包括 VMA 管理、缺页触发分配与读入、共享映射回写，以及 `fork`/`exit` 下的映射处理。

**实验难度：** `mmap/munmap` (hard) · `VMA + 缺页处理路径` (hard)

## `mmap` (hard)

本篇是 [MIT 6.1810 Lab: mmap](https://pdos.csail.mit.edu/6.1810/2025/labs/mmap.html) 的实现笔记。这个 lab 只有一道题，但是经典题目——从 MIT 6.828 到 MIT 6.S081，再到现在的 6.1810，`mmap`/`munmap` 一直都是压轴的最后一个 lab 题。

这个 lab 要求我们在 xv6 里加上 `mmap` 和 `munmap` 这两个系统调用，重点是支持内存映射文件（memory-mapped files）。简单说，就是让一个文件被"映射"进某个进程的地址空间之后，进程可以直接像读写内存一样读写这段地址，背后由内核负责把数据在内存和磁盘文件之间搬运，而不需要用户显式调用 `read`/`write`。

需要特别注意几个点：

- **懒分配（lazy allocation）**：调用 `mmap` 的时候，并不会真的分配物理内存或读取文件内容，只是在进程的地址空间里"预留"一段区域。真正的物理页分配和文件内容加载，发生在第一次访问这段内存、触发 page fault 之后——也就是"用到才加载"。
- **`munmap` 的回收与写回**：`munmap` 负责撤销映射、回收对应的物理内存；如果这段映射是 `MAP_SHARED`，并且内存内容已经被修改过，还需要把这部分内容写回磁盘上的原文件。
- **`fork`/`exit` 时的映射关系**：子进程 `fork` 出来后应该继承父进程的映射关系；进程 `exit` 的时候，也要对它所有的 `MAP_SHARED` 映射做一遍"相当于调用了 `munmap`"的写回和清理。

### 系统调用注册

第一步先把 `mmap` 和 `munmap` 这两个新的系统调用在 xv6 里注册好，步骤和之前 Lab 里面添加系统调用完全一样——还是配置 `kernel/syscall.h`（加系统调用号）、`kernel/syscall.c`（注册到调用表）、`user/user.h` 和 `user/usys.pl`（导出给用户态），最后再把 `mmaptest` 加进 `Makefile` 的 `UPROGS`。这一步先不实现具体逻辑，`sys_mmap`/`sys_munmap` 让直接返回 `-1`，先让 `user/mmaptest.c` 能编译通过跑起来，这个时候运行测试就可以看到它卡在第一个 `mmap` 调用上。

### VMA 数据结构

接下来需要定义一个数据结构 `vma`（virtual memory area）。因为一个进程可能同时映射多个文件，所以需要一个数组来记录当前有哪些区域被映射了、以及每个区域的详细信息。

先在 `kernel/param.h` 里定义好每个进程最多能有多少个 `vma`：

> 每个进程最多同时维护 16 个映射区域——这个数字也是 hint 里建议的大小，对 `mmaptest` 来说足够用。

```h
#define MAXPATH      128   // maximum file path name
#define USERSTACK    1     // user stack pages
#define NVMA         16
```

然后在 `kernel/proc.h` 里定义 `vma` 结构体本身：

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

然后给进程加上 `vma` 结构数组，并把 `vma` 挂到 `struct proc` 上

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

`struct proc` 里新加的 `mmapend` 记录的是可用的分配地址，从这个位置起的内存位置可用。

> proc.h` 里用到了 `size_t`/`off_t`，但没有包含定义它们的头文件
> 需要在开头引一下包 - `#include "defs.h"` 

### `allocproc` / `freeproc` 中的初始化与清理

有了数据结构之后，还需要在进程创建和销毁的地方把这些新字段初始化、清理干净，对应的是 `kernel/proc.c` 里的 `allocproc` 和 `freeproc`。

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

`allocproc` 里新加的两处：一是把 `p->mmapend` 初始化为 `TRAPFRAME`，作为这个进程 `mmap` 区域的起始位置；二是在函数末尾把 `vmas` 数组重置，保证每个新分配的进程里 `vma` 槽位都是干净状态。`freeproc` 则是重置。

### `sys_mmap`

可以开始正式实现函数功能，我将 `sys_mmap`/`sys_munmap` 从 `kernel/sysproc.c` 挪到了 `kernel/sysfile.c`下。原因有二，一这部分和文件更相关的，另外是因为`argfd()` 在 `kernel/sysfile.c` 里被声明成了 `static`，只在这个文件内部可见；而 `sys_mmap` 需要通过用户传入的 `fd` 拿到 `struct file *`（也就是要复用 `argfd`），所以必须和它放在同一个编译单元里（否则链接错误）。

实现如下：(读取用户输入然后在进程里面找合适的槽记进去)

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

如果检查我的代码可以发现最开始我的实现是用从 `PGROUNDUP(p->sz)` 开始分配 `mmap` 地址，并把 `p->sz` 往前推进。
但是这样做的时候在 "test munmap prevents access" 测试里暴露了一个 bug：子进程 `munmap` 掉一页后再访问同一地址，因为 `munmap` 没有回退 `p->sz`，这个地址仍然小于 `p->sz`，导致 `vmfault` 把它误判成合法的堆懒分配、凭空分配一页返回成功，而不是应有的"非法访问、杀进程"；

最后选择方式是让 `mmap` 使用一块完全独立于 `p->sz` 的地址区间（用 `PGROUNDUP` 来从 `TRAPFRAME` 顶往下切），物理上隔离堆和 `mmap` 区域，这样 `munmap` 之后重新访问就不会再被误判成堆访问。完全规避这个问题。

### `vmfault`

和之前的 lab 一样，实际的分配其实是发生在发生在第一次访问这段内存、触发 page fault 之后（懒加载）。

lab 原来的代码已经有了在 `usertrap` 里识别并处理 `mmap` 导致的 page fault，下面是判断入口：

```c
} else if((r_scause() == 15 || r_scause() == 13) &&
          vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
  // page fault on lazily-allocated page
} else {
```

实际的处理逻辑会在 `vmfault(pagetable_t pagetable, uint64 va, int read)` 里面，我们来到 `kernel/vm.c` 里面修改它：

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

`vmfault` 里添加了这一行 `if(va >= TRAPFRAME) return 0;`

因为测试用例会故意测试超出 `MAXVA` 的非法地址来测试，如果没有这层边界检查，这个地址会被直接传给 `ismapped()` → `walk()`，而 `walk()` 对超出 `MAXVA` 的地址会直接 `panic`。因为这个是从 TRAPFRAME 顶往下切的，所以不能完全等同于原来 "检查 `va >= p->sz`" 的位置（会挡住合法的高地址 `mmap` 访问）。

> 因为 `vmfault` 里用到了 `p->vmas[i].file->ip`，但 `vm.c` 没有 `#include "file.h"`。
> `PROT_READ`/`PROT_WRITE` undeclared：需要 `#include "fcntl.h"`。

> 所以需要特别注意 vm 这里的引包

我在文件开头添加了这些引用（同样顺序不能乱）：

```c
#include "defs.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "file.h"
```


### `sys_munmap`

接下来就可以把 memory unmap 的逻辑加上了：

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

`vmaunmap`（写回文件）要用 `ip->size` 而不是 `vma->len`。`vma->len` 是 `mmap` 声明的映射长度，`ip->size` 才是文件的实际大小，两者经常不一致（比如 `mmap(..., PGSIZE*3, ...)` 映射一个只有 1.5 页内容的文件）。如果用 `vma->len` 判断"是否是最后一页"，会导致 `writei` 把无效字节也写回。

> "test mmap dirty" 里 "dirty read #2" 就是专门测试这个的

### `exit` 和真实文件的写入

首先更新一下 `kernel/defs.h` 文件，后面需要用到相关的数据结构和方法：

```c
// add vma handle function
struct proc;
struct vma;
void vmaunmap(struct proc *p, struct vma *vma, uint64 addr, uint64 len);
```

> 注意：这里不能直接引用 `#include "proc.h"`，会导致头文件循环包含。
> 改成只做前置声明 `struct proc;` / `struct vma;`（因为函数声明只用到指针类型，不需要拿到完整定义）就解决了。


修改 `exit`，让进程退出时把 `MAP_SHARED` 的映射写回文件

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


### 最后一步：`fork` 的更新

修改 `fork`，让子进程继承父进程的映射

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


## Lab 9 整体测试

完成所有练习后，执行以下命令对 Lab 9 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-mmap
```

![2026-07-19 20.52.21](./2026-07-19%2020.52.21.png)
---
