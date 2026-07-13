# Lab 5 实验报告

[English](./lab5-xv6-copy-on-write-solution.md) · [中文](./lab5-xv6-copy-on-write-solution-zh.md)

**目标简述：** 在 xv6 中实现 COW fork：初始共享物理页、写入时按需复制，并正确维护物理页引用计数。

**实验难度：** `implement copy-on-write fork`

## Implement copy-on-write fork (hard)

xv6 中的 `fork()` 系统调用会将父进程的所有用户空间内存复制到子进程中。这项复制工作常常被浪费掉：`fork()` 之后通常会在子进程中调用 `exec()`，而 `exec()` 会丢弃复制的内存，导致大部分复制出来的内存根本不会被使用。因此引入 COW（copy-on-write）机制，将物理内存页的分配与复制推迟到真正需要的时候（如果确实需要的话）。

在编写代码之前先理清整体思路：首先需要修改父进程内存复制到子进程这一步，将"直接复制内容到新分配的内存"改为"父子进程都指向同一块物理内存"。为了避免父子进程同时改写同一块内存，需要将该内存标记为只读，只有在真正发生写操作时（即触发 trap，store page fault）才分配新页面、拷贝内容并更新 PTE。其次，关于内存回收：由于此时可能有多个进程依赖同一个物理页面，不能简单地直接回收，因此需要一个引用计数器来记录每个页面被多少个进程使用，只有当没有进程需要该页面时才能将其释放。

### 页面引用计数器

从最简单的页面计数器开始实现，这样不会破坏任何已有逻辑，也留出时间理清后续思路。首先在 `kernel/kalloc.c` 中添加计数器 `counter`。由于每一页物理内存都需要一个计数值，可以用 `PHYSTOP` 和 `KERNBASE` 的差值除以 `PGSIZE` 得出总页数，并据此构建一个数组来记录每一页的引用计数。

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// init counter for all page table
// cow will use this
uint8 counter[(PHYSTOP - KERNBASE)/PGSIZE];
```

接着在 `kernel/riscv.h` 中定义一个宏，方便后续根据物理地址找到对应页面在计数数组中的索引：

```h
#define PA2IDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE) // find the index of specific page
```

然后添加两个简单的方法，并将它们导出供其他部分使用：给定一个地址，将该地址所在页面的引用计数加一或减一，并返回更新后的最新值。这里需要加锁，避免并发写入导致的竞争。

```c
uint8
increfcnt(void *pa)
{
  uint8 n;
  acquire(&kmem.lock);
  counter[PA2IDX(pa)]++;
  n = counter[PA2IDX(pa)];
  release(&kmem.lock);
  return n;
}

uint8
decrefcnt(void *pa)
{
  uint8 n;
  acquire(&kmem.lock);
  counter[PA2IDX(pa)]--;
  n = counter[PA2IDX(pa)];
  release(&kmem.lock);
  return n;
}
```

然后在 `kernel/defs.h` 中导出这两个方法：

```h
void            kfree(void *);
void            kinit(void);
uint8           increfcnt(void *);
uint8           decrefcnt(void *);
```

按照上述逻辑修改 `kfree`：只有当引用计数为零时（代表没有其他进程还在使用这个页面），才能将其真正释放。

```c
void
kfree(void *pa)
{
  struct run *r;
  uint8 count;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  count = decrefcnt(pa);
  if(count > 0){
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
```

初始化与分配部分需要修改两处。首先是刚给进程分配新页面时，应当把计数加一：

```c
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    increfcnt((void*)r);
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}
```

而在初始化阶段，如果没有提前调用 `increfcnt` 设置好计数值，之后 `kfree` 调用 `decrefcnt` 时就会把计数从零减到 -1，由于类型是 `uint8`，会下溢成 255。

```c
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    increfcnt(p);
    kfree(p);
  }
}
```

以上就是引用计数器需要准备的全部内容，接下来看 `uvmcopy` 中"将父进程内存复制到子进程"这一步的修改。

### uvmcopy 的修改

首先需要定义一个 bit 位来记录某个页面正处于 COW 状态，在 `kernel/riscv.h` 中加入：

```h
#define PTE_C (1L << 8)
```

然后修改 `uvmcopy` 函数，如下：

```c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;   // page table entry hasn't been allocated
    if((*pte & PTE_V) == 0)
      continue;   // physical page hasn't been allocated
    
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    if(flags & PTE_W){
      flags = (flags & ~PTE_W) | PTE_C;
    }

    // update the flags for parent record
    *pte = PA2PTE(pa) | flags;

    if(mappages(new, i, PGSIZE, pa, flags) != 0){
      goto err;
    }

    increfcnt((void*)pa);
  }
  return 0;

 err:
  // do the free, but it only cause the counter -1
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

这段代码的逻辑是：取得页面的 `flags` 后，判断该页是否有写权限，如果有，就去掉写权限并将其标记为 COW 状态；然后将对应的物理地址 `pa`（父进程的）与子进程的 pagetable 建立映射关系；最后将该物理地址的引用计数加一。

### usertrap 与 vmfault

接着检查 `usertrap(void)` 函数，可以发现这部分正是留给实现者完成的内容：

```c
} else if((r_scause() == 15 || r_scause() == 13) &&
            vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
    // page fault on lazily-allocated page
```

> `scause` 为 15 表示 store page fault，为 13 表示 load page fault。

以下是修改完成后的 `vmfault` 代码。根据上面的提示可以知道：写操作时传入的 `read` 参数为 0，因此当 `read` 等于 1 时直接 `return 0`；主要改动都发生在 `if(ismapped(pagetable, va))` 这一分支内。

```c
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 pa, mem;
  pte_t *pte;
  uint flags;
  struct proc *p = myproc();

  if (va >= p->sz)
    return 0;
  va = PGROUNDDOWN(va);
  if(ismapped(pagetable, va)) {
    if(read == 1){
      return 0;
    }
    pte = walk(pagetable, va, 0);
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if(flags & PTE_C){
      mem = (uint64) kalloc();
      if(mem == 0){
        return 0;
      }
      // remove cow symbol & add the write premisson back
      flags = (flags & ~PTE_C) | PTE_W;
      memmove((char*)mem, (char*)pa, PGSIZE);
      // use this new mem space and flags to build pte
      *pte = PA2PTE(mem) | flags;
      kfree((void*)pa);
      return mem;
    } else {
      return 0;
    }
  }
  mem = (uint64) kalloc();
  if(mem == 0)
    return 0;
  memset((void *) mem, 0, PGSIZE);
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
    kfree((void *)mem);
    return 0;
  }
  return mem;
}
```

逻辑如下：只有当对已经存在映射的页面进行写操作时（正是子进程真正发生写入的情况），才会用 `va` 找到对应的 `pte`，并检查其是否处于 COW 状态；只有在 COW 状态下，才会新建内存空间并完成三件事：

1. 更新 `pte` 的 flags——这些全新的页面不再需要 COW 标识，并且完全可写；
2. 将旧数据复制过去；
3. 尝试释放旧内存（调用 `kfree`），以减少一次引用计数，因为子进程不再需要这个旧的物理页面。

### 修改 copyout

最后只需要修改 `copyout`，即完成了整个实现。

在 `kernel/vm.c` 中，需要对 `pte` 多加一层判断：如果当前 `pte` 正处于 COW 状态，就调用刚刚修改好的 `vmfault` 为其重新分配对应内存并更新 `pa0`；如果分配失败（`pa0 == 0`），则直接返回 -1。

```c
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;
  
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }

    pte = walk(pagetable, va0, 0);
    if(*pte & PTE_C) {
      pa0 = vmfault(pagetable, va0, 0);
      if(pa0 == 0) {
        return -1;
      }
    } else if ((*pte & PTE_W) == 0){
      // forbid copyout over read-only user text pages.
      return -1;
    }
      
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```

最后再回顾一下，为什么需要修改 `copyout`？原因很简单：内核态向用户内存写入数据时不会触发 page fault，因此需要手动判断该内存页是否处于 COW 状态；如果是，就按照默认逻辑为其重新分配一个页面后再继续写入，所以需要为其额外补上这部分逻辑。

运行一下测试命令`cowtest`，可以得到下面输出：

![2026-07-12 03.26.44](./2026-07-12%2003.26.44.png)

## 想法思考

> 为什么只对物理内存做 COW，而不直接一步到位——直接复制整个父进程的页表结构，等到真正需要的时候再添加新的映射，岂不是更加 lazy、更加简洁？

首先，复制页表数据远比复制用户数据便宜：复制用户数据是复制整个物理内存页里的全部内容，而复制一条页表项只是修改一个八字节的整数。

COW 的整个设计初衷，就是"昂贵的操作（复制真实数据）能拖就拖，甚至最好永远不做（如果这一页从未被写过）"，而"廉价的操作（建立页表映射关系）"本就不值得拖延——拖延它省不了多少成本，反而会带来额外的麻烦。

`fork()` 返回之后，子进程几乎立刻就要做很多"访问已有内存"的事情：它需要从 `fork()` 返回处继续执行代码（访问 text 段）、读写自己的栈（如函数返回地址、局部变量），以及访问继承而来的堆和全局变量。

子进程刚被 `fork()` 出来时还没有主动申请任何新内存，它只是想正常运行下去，读写自己已经存在的栈和代码。如果这些访问对应的 PTE 压根不存在会怎样？会立刻触发 page fault——而且几乎每一页都会触发一次，因为此时子进程的页表几乎是空的。

原本可以在 `fork()` 系统调用内部，用一个内核态循环一次性、批量地建好所有 PTE（这部分成本很低，纯粹是在内核里操作内存），但如果换成延迟映射，就会变成子进程运行过程中一页一页地触发用户态到内核态的 trap（每次 trap 都有上下文切换开销），逐个补建 PTE。这并不会更快，而是把同一件"廉价的事"拆分成了很多次，还额外叠加了 trap 本身的开销，反而更慢。

所以，直接复制整个页表结构、只对物理内存做 COW，是相对更优的方案。

## Lab 5 整体测试

完成所有练习后，执行以下命令对 Lab 5 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-cow
```

![2026-07-12 03.32.45](./2026-07-12%2003.32.45.png)