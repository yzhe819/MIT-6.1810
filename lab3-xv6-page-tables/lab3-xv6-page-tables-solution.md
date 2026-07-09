# Lab 3 Report

[English](./lab3-xv6-page-tables-solution.md) · [中文](./lab3-xv6-page-tables-solution-zh.md)

**Overview:** Explore the RISC-V page table mechanism, implement page-table inspection/printing, speed up `getpid()` via a shared read-only page, and add superpage support in xv6.

**Difficulty:** `inspect a user-process page table` (easy) · `speed up system calls` (easy) · `print a page table` (easy) · `use superpages` (moderate/hard)

## Inspect a user-process page table (easy)

Run `print_pgtbl` as required by the lab to print page-table information:

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

Start from the first line:

```sh
va 0x0 pte 0x21FC885B pa 0x87F22000 perm 0x5B
```

- `va` is the virtual address (virtual page base). For the first line, this page starts at `0x0`, with 4KB page size (`0x0` to `0xfff`), and the next page starts at `0x1000`.
- `pte` (page table entry) stores both translation and permission bits. For `pte = 0x21FC885B`:

```text
hex: 2    1    F    C    8    8    5    B
bin: 0010 0001 1111 1100 1000 1000 0101 1011
```

The low 10 bits are flags: `00 0101 1011`, i.e. `0x5B`, which matches `perm 0x5B`.

- `pa` is the physical address. Since PTE stores a physical page number (PPN), physical page base is obtained by removing low 10 flag bits and restoring page alignment.

For any `va`, the corresponding `pa` is:

```text
PA = (PTE >> 10 << 12) | (VA & 0xFFF)
```

xv6 provides the macro:

```h
#define PTE2PA(pte) (((pte) >> 10) << 12)
```

Comparing the first two rows, `va` differs by only `0x1000`, but `pa` jumps from `0x87F22000` to `0x87F1F000`. This confirms the lab note: xv6 does not map consecutive virtual pages to consecutive physical pages.

> Permission flags from low to high bits are: `PTE_V`, `PTE_R`, `PTE_W`, `PTE_X`, `PTE_U`, `PTE_G`, `PTE_A`, `PTE_D`.

![perm](2026-07-08%2022.28.10.png)

## Speed up system calls (easy)

The goal is to establish a shared read-only region between user and kernel space so some system calls can avoid repeated user-kernel switching overhead.

First, inspect `USYSCALL` in `memlayout.h`:

```c
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
#ifdef LAB_PGTBL
#define USYSCALL (TRAPFRAME - PGSIZE)

struct usyscall {
  int pid;  // Process ID
};
#endif
```

Its organization is similar to `trapframe`. The lab requires storing current process `pid` in this page, and user mode must only have read access.

Following the `trapframe` pattern in `kernel/proc.c`, add `usyscall` to `struct proc`:

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

Initialize it in `allocproc()` (after PID allocation), similar to `trapframe`:

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

Free it in cleanup:

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

Then map `USYSCALL` in `proc_pagetable` (after trampoline mapping), with read-only user permission:

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

Also update unmap in `proc_freepagetable`:

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

For the conceptual question ("what other calls can be sped up this way?"): frequent, read-only, quickly changing values are good candidates, e.g., time/ticks-like queries. If every read requires a kernel crossing, overhead accumulates significantly.

## Print a page table (easy)

The provided `vmprint` scaffold is in `kernel/vm.c`. `pagetable_t` is essentially a `uint64` pointer:

```c
typedef uint64 *pagetable_t; // 512 PTEs
```

Output requirements: print root pagetable first (level 0), then print each deeper level with one more `" .."` prefix, up to 3 levels. Recursive DFS is straightforward. The lab also suggests `freewalk` as reference:

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

Recursive implementation (print all valid entries at all levels):

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

Core logic:

- `(pte & PTE_V) == 0`: invalid entry, skip.
- `(pte & (PTE_R|PTE_W|PTE_X)) == 0`: pointer to lower-level page table, recurse.
- `child` virtual address comes from current level span plus index offset.

## Use superpages (moderate)/(hard)

Superpages reduce PTE count and page-table memory overhead, and may improve access efficiency by covering larger address ranges with one entry.

Start with macros/constants in `kernel/riscv.h`:

```c
#ifdef LAB_PGTBL
#define SUPERPGAMOUNT 16 // assume we only need 16 super page table
#define SUPERPGSIZE (2 * (1 << 20)) // bytes per page
#define SUPERPGROUNDUP(sz)  (((sz)+SUPERPGSIZE-1) & ~(SUPERPGSIZE-1))
```

In `kernel/kalloc.c`, create a dedicated freelist for superpages, and reserve some 2MB-aligned physical regions before normal page allocation:

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

`end` is rounded up to nearest 2MB boundary via `SUPERPGROUNDUP`. The small gap before that boundary is still reused for normal pages, not wasted.

Implement `superfree` / `superalloc` analogous to `kfree` / `kalloc`:

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

Expose declarations in `kernel/defs.h`:

```c
void*           superalloc(void);
void            superfree(void *);
```

Tracing from `sys_sbrk` to `growproc`, actual allocation/free work happens in `uvmalloc` / `uvmdealloc`. Extend `uvmalloc` to choose superpage allocation when range is aligned and large enough:

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

Add dedicated mapping helper and walker. Since superpages map at 2MB granularity, traversal only needs to stop at level-1 entries:

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

`for(int level = 2; level > 1; level--)` effectively runs once (for level 2), then returns the level-1 PTE. In Sv39, level-1 leaf entries can represent 2MB mappings, so you do not descend to level 0 as with 4KB pages.

Then modify `uvmcopy()` and `uvmunmap()`:

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

When `sbrk` frees only part of a superpage (e.g., last 4096 bytes), the superpage must be demoted into regular pages:

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

Finally, fix the provided `SUPERPGROUNDDOWN` macro. The original definition has a boundary bug:

```h
SUPERPGROUNDDOWN(sz) = SUPERPGROUNDUP(sz) - SUPERPGSIZE

Step 1: SUPERPGROUNDUP(0x200000)
= (0x200000 + 0x200000 - 1) & ~(0x200000 - 1)
= 0x3FFFFF & ~0x1FFFFF
= 0x200000        // already aligned

Step 2:
= 0x200000 - 0x200000
= 0x0             // wrong, expected 0x200000
```

Use direct bit masking for round-down:

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

## Full Lab 3 test

After implementing all parts, run the full grader:

```sh
./grade-lab-pgtbl
```

![2026-07-08 20.16.05](./2026-07-08%2020.16.05.png)
