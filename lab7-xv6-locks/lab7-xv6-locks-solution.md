# Lab 7 Report (AI)

**Brief Objective:** Refactor the memory allocator to reduce lock contention (per-core free list for `kalloc`/`kfree` + stealing), and implement a writer-preference read-write spinlock in xv6.

**Lab Difficulty:** `memory allocator` (hard) · `read-write lock` (moderate/hard)

We have arrived at a classic chapter: locks. This is a core component of operating systems, and the truly interesting and hardcore part begins here.

## memory allocator (hard)

The original memory allocator in xv6 uses a single global lock to protect a global free list, `kmem.freelist`. When multiple cores call `kalloc`/`kfree` concurrently, all cores compete for this single lock, making lock contention a performance bottleneck. This requires a redesign.

The idea is to replace the "single global list + single lock" design with a "per-CPU free list + per-CPU lock" design. When different cores operate on their own lists, they do not block each other, allowing tasks to be distributed across CPUs and executed concurrently.

With this in mind, we can begin the modification. We change the global lock to an array with a size equal to the number of CPUs (using the global constant `NCPU`). During initialization, we initialize each lock in the array one by one:

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++) {
    char lockname[8];
    snprintf(lockname, sizeof(lockname), "kmem%d", i);
    initlock(&kmem[i].lock, lockname);
  }
  freerange(end, (void *)PHYSTOP);
}
```

Correspondingly, we also modify the release logic to support the array structure. We free memory to the free list of the current CPU.

```c
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  push_off();
  int id = cpuid();
  pop_off();

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
}
```

Note how we get the CPU ID here: `cpuid()` is only safe to call when interrupts are disabled, so we need to wrap it with `push_off()`/`pop_off()`:

```c
push_off();
int id = cpuid();
pop_off();
```

Next is the challenging part of memory allocation: how to allocate free memory across different CPUs.

The solution is straightforward. The system does not allocate memory in advance, because `freerange` is only called once for the booting core in `kinit`. Thus, all initial free pages are attached to CPU 0's free list. This is a deliberate design; the free lists of other cores are initially empty and must "steal" memory to populate themselves. Then, whenever a new task arrives, it first checks its corresponding CPU ID and looks for free pages in its own list. If it has none, it iterates through other CPUs to find the first one with free memory and "steals" its entire free list (for simplicity, we choose to steal the entire list at once rather than just one page).

> kalloc: If you don't have any, "steal" some memory from neighboring CPUs.

```c
void *
kalloc(void)
{
  struct run *r;
  struct run *steal;

  push_off();
  int id = cpuid();
  pop_off();

  acquire(&kmem[id].lock);
  r = kmem[id].freelist;

  if(r) {
    kmem[id].freelist = r->next;
    release(&kmem[id].lock);
  } else {
    release(&kmem[id].lock);
    for(int i = 0; i < NCPU; i++) {
      if(i == id) {
        continue;
      }

      acquire(&kmem[i].lock);
      if(kmem[i].freelist) {
        steal = kmem[i].freelist;
        kmem[i].freelist = 0;
        release(&kmem[i].lock);

        acquire(&kmem[id].lock);
        kmem[id].freelist = steal;
        r = kmem[id].freelist;
        if(r) {
          kmem[id].freelist = r->next;
          release(&kmem[id].lock);
        }

        break;
      }
      release(&kmem[i].lock);
    }
  }

  if(r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
```

Similarly, we need to wrap the CPU ID retrieval with `push_off()`/`pop_off()` as before:

```c
push_off();
int id = cpuid();
pop_off();
```

Throughout this process, at most one `kmem` lock is held at any given time (we never hold two simultaneously), so there is no risk of deadlock.

At this point, the `memory allocator` part is complete. Following the instructions, running `kalloctest/usertests sbrkmuch` in the xv6 system yields results similar to the image below, indicating a successful implementation.

![2026-07-17 23.44.39](./2026-07-17%2023.44.39.png)

![2026-07-17 23.44.49](./2026-07-17%2023.44.49.png)

Finally, check `usertests` again to see if any original functionality was affected:

![2026-07-17 23.46.41](./2026-07-17%2023.46.41.png)

## read-write lock (moderate/hard)

To implement a read-write lock (modifying `initrwlock`, `read_acquire`, `read_release`, `write_acquire`, and `write_release`), the requirements are:

- Multiple readers can hold the lock simultaneously.
- Writers have exclusive access (when there is a writer, there can be no other readers/writers).
- Writers cannot be starved: once a writer is waiting, any subsequent readers must line up behind it and cannot continuously preempt it.

First, add three fields to `rwspinlock`: the number of readers, whether a writer is writing, and how many writers are queuing up:

```c
struct rwspinlock {
  // Replace this with your implementation.
  struct spinlock l;
  int readers;
  int writing;
  int waiting; // this means the waiting for write operation
};
```

Then comes the simple initialization, setting everything to 0:

```c
void
initrwlock(struct rwspinlock *rwlk)
{
  // Replace this with your implementation.
  initlock(&rwlk->l, "rwlk");
  rwlk->readers = 0;
  rwlk->writing = 0;
  rwlk->waiting = 0;
}
```

Next, let's look at requesting a read lock. We wrap the check in a `while` loop. A reader can only enter if there are no writers waiting and no writer is currently writing. If so, we increment the reader count; otherwise, it continues to wait.

```c
read_acquire_inner(struct rwspinlock *rwlk)
{
  while(true) {
    acquire(&rwlk->l);
    if(rwlk->waiting > 0 || rwlk->writing == 1) {
      release(&rwlk->l);
    } else {
      rwlk->readers++;
      release(&rwlk->l);
      break;
    }
  }
}
```

Releasing the read lock is simple: just decrement the reader count by 1, using the underlying lock to ensure atomicity throughout:

```c
static void
read_release_inner(struct rwspinlock *rwlk)
{
  acquire(&rwlk->l);
  rwlk->readers--;
  release(&rwlk->l);
}
```

Requesting a write lock requires a bit more thought. First, it needs to "queue up" to block subsequent readers, and then wait for existing readers to finish.

Key logic: When a writer arrives, it first adds itself to the waiting queue (`waiting++`). This step immediately blocks any readers trying to cut in line. Then, it only truly acquires the write lock when "no one is writing and no readers are reading":

```c
static void
write_acquire_inner(struct rwspinlock *rwlk)
{
  acquire(&rwlk->l);
  rwlk->waiting++;
  release(&rwlk->l);

  while(true) {
    acquire(&rwlk->l);
    if(rwlk->writing == 0 && rwlk->readers == 0) {
      rwlk->waiting--;
      rwlk->writing = 1;
      release(&rwlk->l);
      break;
    } else {
      release(&rwlk->l);
    }
  }
}
```

The separation of `waiting++` and the actual check for writing is crucial for preventing starvation. As long as a writer has "checked in," `waiting > 0` becomes true, preventing new readers from entering, even if the writer itself still has to wait for existing readers to finish.

Releasing the write lock is as simple as updating the writing state to indicate that no one is writing.

```c
static void
write_release_inner(struct rwspinlock *rwlk)
{
  acquire(&rwlk->l);
  rwlk->writing = 0;
  release(&rwlk->l);
}
```

Once completed, test your rwspinlock implementation by running `rwlktest`. You should see output similar to the following:

![2026-07-18 00.42.05](./2026-07-18%2000.42.05.png)

## Lab 7 Overall Testing

After completing all exercises, execute the following command to run a full test on Lab 7, verifying the correctness of each implementation:

```sh
./grade-lab-locks
```

![2026-07-18 14.02.04](./2026-07-18%2014.02.04.png)
