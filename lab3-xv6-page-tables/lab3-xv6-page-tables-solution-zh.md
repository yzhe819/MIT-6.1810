# Lab 3 实验报告

**目标简述：** 探索 RISC-V 页表机制，完成页表检查/打印、通过共享只读页优化 `getpid()`，并在 xv6 中实现 superpage（大页）支持。

**实验难度：** `inspect a user-process page table` (easy) · `speed up system calls` (easy) · `print a page table` (easy) · `use superpages` (moderate/hard)

## Inspect a user-process page table (easy)

按照题目要求运行 `print_pgtbl` 函数以打印页表信息，结果如下：

```sh
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ pgtbltest
print_pgtbl starting
va 0x0 pte 0x21FC885B pa 0x87F22000 perm 0x5B
va 0x1000 pte 0x21FC7C5B pa 0x87F1F000 perm 0x5B
va 0x2000 pte 0x21FC7817 pa 0x87F1E000 perm 0x17
va 0x3000 pte 0x21FC7407 pa 0x87F1D000 perm 0x7
va 0x4000 pte 0x21FC70D7 pa 0x87F1C000 perm 0xD7
va 0x5000 pte 0x0 pa 0x0 perm 0x0
va 0x6000 pte 0x0 pa 0x0 perm 0x0
va 0x7000 pte 0x0 pa 0x0 perm 0x0
va 0x8000 pte 0x0 pa 0x0 perm 0x0
va 0x9000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFF6000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFF7000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFF8000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFF9000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFA000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFB000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFC000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFD000 pte 0x0 pa 0x0 perm 0x0
va 0x3FFFFFE000 pte 0x21FD08C7 pa 0x87F42000 perm 0xC7
va 0x3FFFFFF000 pte 0x2000184B pa 0x80006000 perm 0x4B
print_pgtbl: OK
ugetpid_test starting
usertrap(): unexpected scause 0xd pid=4
            sepc=0x7b2 stval=0x3fffffd000
$ 
```

从输出的第一行开始分析：

```sh
va 0x0 pte 0x21FC885B pa 0x87F22000 perm 0x5B
```

`va` 表示虚拟地址（virtual address），对应该虚拟页面的起始地址。以第一条记录为例，页面起始地址为 `0x0`，页面大小为 4KB（对应地址范围 `0x0` 到 `0xfff`），下一页的起始地址即为 `0x1000`。

`pte`（page table entry）是页表中的一条记录，负责将虚拟地址翻译为物理地址，并记录该页面的访问权限。以 `pte = 0x21FC885B` 为例，展开如下：

```
十六进制:  2    1    F    C    8    8    5    B
二进制:  0010 0001 1111 1100 1000 1000 0101 1011
```

其中最低的 10 位为权限位，即 `00 0101 1011`，转换为十六进制为 `0x5B`，对应该条记录末尾的 `perm 0x5B`。

`pa` 为物理地址，`pte` 中存储的是 PPN（物理页号），可通过将 `pte` 右移 10 位去除权限位、再左移 12 位得到对应的物理页起始地址。

若已知某个特定的 `va`，可通过以下公式计算出对应的 `pa`（先算出页面起始物理地址，再加上页内偏移量）：

```
PA = (PTE >> 10 << 12) | (VA & 0xFFF)
```

xv6 里面有对应的宏用了上面的公式

```h
#define PTE2PA(pte) (((pte) >> 10) << 12)
```

对照第一条和第二条记录可以发现，`va` 仅相差 `0x1000`，但 `pa` 却从 `0x87F22000` 跳变为 `0x87F1F000`，这正对应题目所说明的现象：xv6 不会将用户虚拟页连续地映射到物理内存，因此对应的 `pa` 并不连续。

> 此处应补充权限位（perm）各标志位的具体含义说明，从低到高位分别是 `PTE_V`、`PTE_R`、`PTE_W`、`PTE_X`、`PTE_U`、`PTE_G`、`PTE_A`、`PTE_D`

![perm](2026-07-08%2022.28.10.png)

## Speed up system calls (easy)

首先需要建立一个能够让用户态与内核态之间共享读取数据的区域，从而加速系统调用操作，避免频繁在用户态与内核态之间切换。根据题目提示，首先查看 `USYSCALL` 的数据结构，其定义位于 `memlayout.h` 中：

```c
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
#ifdef LAB_PGTBL
#define USYSCALL (TRAPFRAME - PGSIZE)

struct usyscall {
  int pid;  // Process ID
};
#endif
```

该结构体的组织方式与 `trapframe` 类似。题目要求将当前进程的 `pid` 存入 `usyscall` 页面，并且仅允许用户态对该页面执行读取操作（避免赋予多余权限带来安全隐患）。

参考 `kernel/proc.c` 中对 `trapframe` 的处理方式，为 `usyscall` 在 `proc` 结构体中同样保留一份记录，代码如下：

```c
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
  struct usyscall *usyscall;   // data page for USYSCALL.
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};
```

随后在分配 `proc` 的过程中一并初始化 `usyscall`，处理方式与 `trapframe` 一致；需要注意的是，必须在获取到分配好的 `pid` 之后，才能将该 `pid` 写入 `usyscall`：

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

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Allocate a USYSCALL page.
  if((p->usyscall = (struct usyscall *)kalloc()) == 0){
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

  // initialize the USYSCALL page.
  memset(p->usyscall, 0, sizeof(struct usyscall));
  p->usyscall->pid = p->pid;

  return p;
}
```

同样需要在清理逻辑中加入对应的释放操作：

```c
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->usyscall)
    kfree((void*)p->usyscall);
  p->usyscall = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}
```

接下来需要将 `usyscall` 对应的页表项与实际物理内存关联起来，即完成映射（mapping）。在 `proc_pagetable` 函数中（该函数用于为指定 `proc` 分配页表），紧跟 `trampoline` 的映射之后加入 `USYSCALL` 的映射；若映射失败，需要清理之前已完成的步骤。需要特别注意：此处仅允许用户态读取，不应赋予额外权限：

```c
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the USYSCALL page just below the trampoline page, for
  // USYSCALL.
  // user can access and read this USYSCALL page. but cannot write to it.
  if(mappages(pagetable, USYSCALL, PGSIZE,
              (uint64)(p->usyscall), PTE_R | PTE_U) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, USYSCALL, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}
```

同样，也需要处理释放时对应的逻辑：

```c
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmunmap(pagetable, USYSCALL, 1, 0);
  uvmfree(pagetable, sz);
}
```

回到 lab 提出的问题：哪些系统调用可以通过这种机制被加速？答案是那些被频繁调用、且返回值频繁变化的只读型系统调用，例如获取当前时间或进程运行时长（类似 `time` 系统调用），这类数据可能每个 tick 都会发生变化。若每次读取都需要切换到内核态再返回用户态，这种上下文切换的开销在高频调用场景下会被显著放大；而通过共享页面直接读取，则可以省去这部分切换成本。

## Print a page table (easy)

首先定位已提供框架的 `vmprint` 函数，位于 `kernel/vm.c` 中，其参数 `pagetable_t pagetable` 的类型本质上是一个 `uint64` 指针：

```c
typedef uint64 *pagetable_t; // 512 PTEs
```

查看题目要求的输出格式：第一行直接打印当前的 `pagetable` 即可（对应第 0 层），此后每进入下一层，行首多加一组 `" .."` 前缀，最大深度为三层。使用递归实现该逻辑较为直接。题目同时提示可以参考 `freewalk` 函数的实现方式：

题目提示：`Use %p in your printf calls to print out full 64-bit hex PTEs and addresses as shown in the example.`

```c
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      // backtrace();
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}
```

整体递归结构与 `freewalk` 类似，区别在于不对叶子页表项做限制性判断，而是将所有层级的页表项全部打印出来：

```c
#if defined(LAB_PGTBL) || defined(SOL_MMAP) || defined(SOL_COW)
void 
_vmprint(pagetable_t pagetable, uint64 va, int level) {
  if (level > 3) {
    return;
  }

  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    // 1ULL => 1 unsigned long long
    uint64 span = 1ULL << PXSHIFT(3 - level);
    uint64 child = va + i * span;
    pte_t pte = pagetable[i];
    uint64 pa = PTE2PA(pte);

    if((pte & PTE_V) == 0)
      continue;

    // print information
    for(int j = 0; j < level; j++){
      printf(" ..");
    }

    printf("%p: pte %p pa %p\n", (void*)child, (void*)pte, (void*)pa);

    if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // points to a lower-level page table
      _vmprint((pagetable_t)PTE2PA(pte), child, level + 1);
    }
  }
}

void
vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  _vmprint(pagetable, 0, 1);
}
#endif
```

简要说明判断逻辑：`(pte & PTE_V) == 0` 表示该页表项无效（空表项），直接跳过；`(pte & (PTE_R|PTE_W|PTE_X)) == 0` 表示该页表项指向下一级页表，此时需要递归调用（DFS 深度优先遍历）。较为绕的一点在于如何通过 `level` 和 `i` 计算出子节点对应的虚拟地址 `child`：

每一层对应的地址跨度需要根据该层级左移相应的位数（`PXSHIFT`），再乘以当前页表项在本级 512 个页表项中的索引 `i`，二者相加即可得到该页表项所覆盖虚拟地址范围的起始地址。

## Use superpages (moderate)/(hard)

引入超级页（superpage）的动机在于：页表本身也需要占用内存。若采用普通大页组织内存，会需要大量的 `pte` 记录，且这些 `pte` 分散在多级页表结构中，访问效率较低；而使用超级页后，单条页表项即可覆盖更大的地址范围，无需维护大量 `pte`，从而降低存储开销、提升访问效率。

首先定位与超级页相关的宏定义与常量，包括所需的超级页数量，位置在 `kernel/riscv.h` 中：

```c
#ifdef LAB_PGTBL
#define SUPERPGAMOUNT 16 // assume we only need 16 super page table
#define SUPERPGSIZE (2 * (1 << 20)) // bytes per page
#define SUPERPGROUNDUP(sz)  (((sz)+SUPERPGSIZE-1) & ~(SUPERPGSIZE-1))
```

随后进入 `kernel/kalloc.c`，仿照普通页表的组织方式定义对应的数据结构，并在分配普通页表之前，预先从物理内存中划出一部分地址空间留给超级页使用：

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  struct run *freelist;
} supermem;

void
kinit()
{
  // init lock
  initlock(&kmem.lock, "kmem");
  initlock(&supermem.lock, "supermem");
  // round up to the near 2MB bound
  char *pgstart = (char*)SUPERPGROUNDUP((uint64)end);

  // the small leftover before the aligned region
  freerange(end, pgstart);

  // carve out SUPERPGAMOUNT * 2MB region for superpages
  supermem.freelist = 0;
  char *p = pgstart;
  for(int i = 0; i < SUPERPGAMOUNT; i++){
    struct run *r = (struct run*)p;
    r->next = supermem.freelist;
    supermem.freelist = r;
    p += SUPERPGSIZE; // exactly 2MB
  }

  // the reminder memory will be used as the normal one
  freerange(p, (void*)PHYSTOP);
}
```

上述代码的核心逻辑：首先将起始地址 `end` 向上对齐到最近的 2MB 边界，使用宏 `SUPERPGROUNDUP` 完成；`end` 到该对齐起始地址之间的这部分“零头”内存并不会被浪费，而是继续分配给普通页表使用。

之后根据设定的数量 `SUPERPGAMOUNT` 依次划分出对应的超级页区域；划分完成后，剩余的内存全部交给普通页表使用。

接下来对照 `kfree` 与 `kalloc`，实现超级页对应的版本：

```c

void
superfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % SUPERPGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("superfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, SUPERPGSIZE);

  r = (struct run*)pa;

  acquire(&supermem.lock);
  r->next = supermem.freelist;
  supermem.freelist = r;
  release(&supermem.lock);
}

void *
superalloc(void)
{
  struct run *r;

  acquire(&supermem.lock);
  r = supermem.freelist;
  if(r)
    supermem.freelist = r->next;
  release(&supermem.lock);

  if(r)
    memset((char*)r, 1, SUPERPGSIZE); // fill with junk
  return (void*)r;
}
```

同时需要将这两个方法导出，以便测试程序调用，在 `kernel/defs.h` 中添加声明：

```c
void*           superalloc(void);
void            superfree(void *);
```

回到题目本身，题目建议以 `kernel/sysproc.c` 中的 `sys_sbrk` 函数作为切入点，该函数由 `sbrk` 系统调用触发。沿着调用路径可以找到 `growproc` 函数，它负责为 `sbrk` 预先分配内存。

顺着 `sbrk` 追踪到 `growproc` 后可以发现，其内部实际调用了 `uvmalloc` 与 `uvmdealloc` 完成分配与释放。先看 `uvmalloc` 部分：在原有分配逻辑基础上增加一个判断——如果满足超级页的分配条件，则调用新增的超级页方法；否则仍沿用原有逻辑：

```c
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  int sz;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += sz){
    if(a == SUPERPGROUNDDOWN(a) && newsz - a >= SUPERPGSIZE){
      sz = SUPERPGSIZE;
      mem = superalloc();
    } else {
      sz = PGSIZE;
      mem = kalloc();
    }
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
#ifndef LAB_SYSCALL
    memset(mem, 0, sz);
#endif
    if(sz == SUPERPGSIZE){
      if(mapsuperpages(pagetable, a, sz, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
        superfree(mem);
        uvmdealloc(pagetable, a, oldsz);
        return 0;
      }
    }else{
      if(mappages(pagetable, a, sz, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
        kfree(mem);
        uvmdealloc(pagetable, a, oldsz);
        return 0;
      }
    }
  }
  return newsz;
}
```

针对超级页新增了一个专门的映射方法，其内部同样用到了类似 `walk` 的地址转换逻辑，这里改为 `superwalk`。由于超级页是直接分配到 2MB 粒度的物理页，因此地址转换只需要两级，而非普通页表的三级。以下是新增的 `mapsuperpages` 和 `superwalk` 方法：

> 需要说明的是，`superwalk` 的 `else` 分支处理的是页表页（page table page）本身不存在的情况——此时需要新分配一个页表页。该页表页始终是标准的 512 个 `pte` 组织形式，大小固定为 4KB，因此这里沿用 `PGSIZE` 是正确的，无需改动。

```c
// only used for handle super pages
pte_t *
superwalk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("superwalk");

  for(int level = 2; level > 1; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
#ifdef LAB_PGTBL
      if(PTE_LEAF(*pte)) {
        return pte;
      }
#endif
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(1, va)];
}

int
mapsuperpages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if((va % SUPERPGSIZE) != 0)
    panic("mapsuperpages: va not aligned");

  if((size % SUPERPGSIZE) != 0)
    panic("mapsuperpages: size not aligned");

  if(size == 0)
    panic("mapsuperpages: size");
  
  a = va;
  last = va + size - SUPERPGSIZE;
  for(;;){
    if((pte = superwalk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mapsuperpages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += SUPERPGSIZE;
    pa += SUPERPGSIZE;
  }
  return 0;
}
```

> `superwalk` 中的 `for(int level = 2; level > 1; level--)` 实际只会执行一次（`level = 2`），随后直接返回第 1 级页表中的 `pte`。这是因为在 Sv39 分页机制下，第 1 级页表项本身即可作为叶子节点（leaf），对应覆盖 2MB 的地址范围，因此超级页的地址转换只需经过第 2 级到第 1 级这一步，无需像普通 4KB 页那样再下探到第 0 级。(只有两层)

接下来根据题目要求修改 `uvmcopy()` 与 `uvmunmap()`：

```c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;
  int szinc = PGSIZE;

  for(i = 0; i < sz; i += szinc){
    pte = superwalk(old,i,0);

    // check if the super page 
    if(pte != 0 && (*pte & PTE_V) && PTE_LEAF(*pte)){
      szinc = SUPERPGSIZE;

      pa = PTE2PA(*pte);
      flags = PTE_FLAGS(*pte);
      if((mem = superalloc()) == 0)
        goto err;
      memmove(mem, (char*)pa, SUPERPGSIZE);
      if(mapsuperpages(new, i, SUPERPGSIZE, (uint64)mem, flags) != 0){
        superfree(mem);
        goto err;
      }
    }else{
      if((pte = walk(old, i, 0)) == 0)
        continue;
      if((*pte & PTE_V) == 0) {
        continue;
      }
      szinc = PGSIZE;

      pa = PTE2PA(*pte);
      flags = PTE_FLAGS(*pte);
      if((mem = kalloc()) == 0)
        goto err;
      memmove(mem, (char*)pa, PGSIZE);
      if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
        kfree(mem);
        goto err;
      }
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;
  int sz = PGSIZE;
  uint64 end = va + npages*PGSIZE;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += sz){
    pte = superwalk(pagetable, a, 0);
    if(pte != 0 && (*pte & PTE_V) && PTE_LEAF(*pte)){
      uint64 supstart = SUPERPGROUNDDOWN(a);
      uint64 supend = supstart + SUPERPGSIZE;
      sz = SUPERPGSIZE;

      // release the entire super pg
      if(supstart >= va && supend <= end){
        if(do_free){
          uint64 pa = PTE2PA(*pte);
          superfree((void*)pa);
        }
        *pte = 0;
        continue;
      } else {
        // demote the entire page to multiple normal pages
        if(superpg_demote(pagetable, supstart) != 0)
          panic("uvmunmap: super pg demote failed");
        pte = walk(pagetable, a, 0);
      }
    }

    if((pte = walk(pagetable, a, 0)) == 0) // leaf page table entry allocated?
      continue;
    if((*pte & PTE_V) == 0)  // has physical page been allocated?
      continue;
    sz = PGSIZE;
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```

当 `sbrk` 释放部分超级页时（例如仅释放该超级页最后 4096 字节所对应的区域），需要将整个超级页“降级”为若干个普通页，因此新增了对应的降级函数：

```c
int
superpg_demote(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 oldpa;
  uint flags;
  char *mem;
  int i;

  if((va % SUPERPGSIZE) != 0)
    panic("superpg_demote: va not aligned");

  pte = superwalk(pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0 || !PTE_LEAF(*pte))
    panic("superpg_demote: not a superpage");

  oldpa = PTE2PA(*pte);
  flags = PTE_FLAGS(*pte);

  // Clear the superpage PTE first so mappages() can create a
  // level-0 page table under it (walk() will allocate one, since
  // the level-1 PTE below no longer looks like a leaf).
  *pte = 0;

  for(i = 0; i < SUPERPGSIZE; i += PGSIZE){
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)(oldpa + i), PGSIZE);
    if(mappages(pagetable, va + i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }

  superfree((void*)oldpa);
  return 0;

 err:
  // best-effort cleanup of what we've mapped so far
  uvmunmap(pagetable, va, i / PGSIZE, 1);
  return -1;
}
```

最后一步，需要修正题目原本给出的宏定义，原因在于给定的 `SUPERPGROUNDDOWN` 存在边界处理缺陷。

具体分析 `SUPERPGROUNDDOWN` 的实现：它先调用 `SUPERPGROUNDUP`，再减去一个完整的 `SUPERPGSIZE`。当地址本身没有对齐到 `SUPERPGSIZE` 时，这种做法是合理的；但当地址恰好已经对齐（例如地址正好是 2MB）时，`SUPERPGROUNDUP` 会原样返回该地址（因为它已经是对齐的），随后再减去一个 `SUPERPGSIZE`，结果就相当于多减了一整个超级页的大小——即向下取整反而跳过了正确的边界。

```h
SUPERPGROUNDDOWN(sz) = SUPERPGROUNDUP(sz) - SUPERPGSIZE

第一步 SUPERPGROUNDUP(0x200000):
= (0x200000 + 0x200000 - 1) & ~(0x200000 - 1)
= 0x3FFFFF & ~0x1FFFFF
= 0x200000        // 已经对齐，向上取整还是它自己

第二步：
= 0x200000 - 0x200000
= 0x0             // ← 错误！本应还是 0x200000
```

因此需要将该宏改为直接使用位运算完成向下取整：

```h
#ifdef LAB_PGTBL
#define SUPERPGAMOUNT 16 // assume we only need 5 super page table
#define SUPERPGSIZE (2 * (1 << 20)) // bytes per page
#define SUPERPGROUNDUP(sz)  (((sz)+SUPERPGSIZE-1) & ~(SUPERPGSIZE-1))
// the following is the given one, this will delete one more super page size
// when sz is match the super page size, for example, at 2mb it will still keep 2mb after superpgroundup
// then it accidently delete one more 2mb, the sz will be 0mb 
// #define SUPERPGROUNDDOWN(sz) (SUPERPGROUNDUP(sz)-SUPERPGSIZE)
#define SUPERPGROUNDDOWN(sz) ((sz) & ~(SUPERPGSIZE-1)) // correct bit operation
#endif
```

## Lab 3 整体测试

完成所有练习后，执行以下命令对 Lab 3 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-pgtbl
```

![2026-07-08 20.16.05](./2026-07-08%2020.16.05.png)