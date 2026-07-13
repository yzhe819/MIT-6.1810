# Lab 5 Report

[English](./lab5-xv6-copy-on-write-solution.md) · [中文](./lab5-xv6-copy-on-write-solution-zh.md)

**Overview:** Implement COW fork in xv6 by sharing physical pages at first, copying on demand when writes happen, and correctly maintaining physical-page reference counts.

**Difficulty:** `implement copy-on-write fork`

## Implement copy-on-write fork (hard)

In xv6, the `fork()` system call copies all user-space memory from the parent into the child. This copy work is often wasted: after `fork()`, the child usually calls `exec()`, and `exec()` discards the copied memory. That means most copied pages are never used.  
So we introduce COW (copy-on-write): delay physical-page allocation and copying until it is truly needed (if it is needed at all).

Before coding, it helps to make the full flow clear:

1. In the parent→child memory duplication step, change from "copy data into newly allocated pages" to "parent and child both point to the same physical pages."
2. To prevent parent/child from writing the same page at the same time, mark those pages read-only.
3. Only when a write actually happens (trap: store page fault), allocate a new page, copy content, and update the PTE.
4. For memory reclamation, because multiple processes may share the same physical page, we cannot free directly. We need reference counts per page and free only when no process still needs that page.

### Page reference counter

Start with the simplest part: page reference counting.  
This can be implemented first without breaking existing logic, and it gives a clean base for later COW changes.

In `kernel/kalloc.c`, add a counter array.  
Each physical page needs one counter entry, so we compute page count by `(PHYSTOP - KERNBASE) / PGSIZE`.

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// init counter for all page table
// cow will use this
uint8 counter[(PHYSTOP - KERNBASE)/PGSIZE];
```

Then in `kernel/riscv.h`, add a helper macro to map physical address → counter index:

```h
#define PA2IDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE) // find the index of specific page
```

Next, add two helper functions and export them for other code to use:
- increment refcount for the page containing `pa`
- decrement refcount for the page containing `pa`
- return the updated value

Locking is required to avoid races.

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

Export them in `kernel/defs.h`:

```h
void            kfree(void *);
void            kinit(void);
uint8           increfcnt(void *);
uint8           decrefcnt(void *);
```

Now update `kfree`: only really free the page when refcount becomes zero (no process is still using it).

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

Allocation and initialization need two changes:

1. When a brand-new page is allocated to a process, increment refcount:

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

2. During init, if we do not set counters first, `kfree` will call `decrefcnt` from zero and underflow to 255 (`uint8` wraparound).  
So in `freerange`, increment first, then `kfree`:

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

That is everything needed for the page reference counter.  
Next, we modify the parent→child memory-copy step in `uvmcopy`.

### Changes to `uvmcopy`

First, define one PTE bit to mark COW state in `kernel/riscv.h`:

```h
#define PTE_C (1L << 8)
```

Then modify `uvmcopy`:

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

Logic summary:
- Read page `flags`.
- If writable, remove write permission and mark as COW.
- Map the same parent physical page `pa` into child page table.
- Increment refcount for that physical page.

### `usertrap` and `vmfault`

In `usertrap(void)`, this part is exactly where we handle page faults:

```c
} else if((r_scause() == 15 || r_scause() == 13) &&
            vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
    // page fault on lazily-allocated page
```

> `scause == 15` means store page fault, `scause == 13` means load page fault.

Below is the updated `vmfault`.  
From the call above, write faults pass `read=0`, so if `read==1`, we return early.  
The key changes are in `if (ismapped(pagetable, va))`.

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

The logic is:
- Only when writing to an already mapped page (the real child-write case), we find the PTE and check COW state.
- Only for COW pages, allocate a new page and do three things:
  1. update flags (remove COW mark, restore write permission),
  2. copy old data to new page,
  3. try to free old page (`kfree`) to drop one reference, since child no longer needs that old physical page.

### Change `copyout`

Finally, we still need one last change: `copyout`.

In `kernel/vm.c`, add one more PTE check:
- if current page is COW, call `vmfault` to allocate/update page and refresh `pa0`
- if allocation fails (`pa0 == 0`), return `-1`

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

Why modify `copyout`?  
Because kernel-mode writes into user memory do not trigger user page faults.  
So we must manually check whether the target page is COW. If yes, we must do the COW split first, then continue writing.

Run `cowtest`, expected output:

![2026-07-12 03.26.44](./2026-07-12%2003.26.44.png)

## Design discussion

> Why do COW only for physical memory? Why not go even more lazy and avoid copying the full parent page-table structure too, and only build mappings when really needed?

First, copying page-table metadata is much cheaper than copying user data.  
Copying user data means copying full physical-page contents.  
Copying one PTE is just updating one 8-byte integer.

COW is meant to postpone expensive work (real data copies), ideally forever for pages that are never written.  
Cheap work (setting up mappings) is not worth postponing: little cost saved, but more complexity introduced.

After `fork()`, the child almost immediately accesses existing memory:
- it resumes execution at the return point (text segment),
- reads/writes its stack (return address, locals),
- accesses inherited heap/global memory.

A freshly forked child has not requested any brand-new memory yet. It just needs to keep running with existing memory.  
If those PTEs do not exist, what happens? Immediate page faults, likely one per page, because the child page table would be mostly empty.

What could have been done in one cheap kernel loop during `fork()` (bulk PTE setup) becomes many user→kernel traps at runtime, one page at a time.  
That does not make things faster. It splits one cheap operation into many operations and adds trap/context-switch overhead, often making it slower.

So the better approach is:
- copy page-table structure eagerly,
- apply COW only to physical-page data.

## Lab 5 full test

After finishing all tasks, run the full Lab 5 test suite to validate correctness:

```sh
./grade-lab-cow
```

![2026-07-12 03.32.45](./2026-07-12%2003.32.45.png)
