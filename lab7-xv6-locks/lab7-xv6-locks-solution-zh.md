# Lab 7 实验报告

**目标简述：** 重构内存分配器以降低锁竞争（`kalloc`/`kfree` 每核空闲链表 + stealing），并在 xv6 中实现写者优先的读写自旋锁。

**实验难度：** `memory allocator` (hard) · `read-write lock` (moderate/hard)

我们来到了比较经典的一章，锁，操作系统非常核心的部分，真正有趣硬核的环节来了。

## memory allocator (hard)

xv6 原本的内存分配器用一把全局锁保护一条全局空闲链表 `kmem.freelist`。多核并发调用 `kalloc`/`kfree` 时，所有核心都在抢同一把锁，锁竞争成了性能瓶颈，需要改造。

思路是把"一条全局链表 + 一把锁"拆成"每个 CPU 一条空闲链表 + 一把自己的锁"：不同核心操作各自的链表时互不阻塞，任务就能真正分散到各个 CPU 上并发执行。

想清楚这一点就开始改造，把全局锁改成一个数组，大小等于 CPU 数量（用全局常量 `NCPU`），初始化时逐个初始化数组里的每一把锁：

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

然后对应的，把释放逻辑也改一下，改成支持数组的形式。把空闲内存释放到自己所在 CPU 的链表。

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

注意这里取 CPU 号的写法：`cpuid()` 只有在关中断的情况下调用才安全，所以要用 `push_off()`/`pop_off()` 包裹一下：

```c
push_off();
int id = cpuid();
pop_off();
```

然后就是内存分配的难点了，怎么在不同的cpu下分配空闲内存呢。

解决方法很简单，系统先不提前分配，因为 `kinit` 里只对当前启动核调用了一次 `freerange`，所有初始的空闲页都会挂在 0 号 CPU 的链表上——这也是刻意的设计，其他核心的链表一开始是空的，要靠"偷"内存来补上。然后每当新的任务进来的时候，找到自己对应的 CPU 号，先看自己链表里有没有空闲页；如果没有，就遍历其他 CPU，找到第一个有空闲内存的，把它的整条链表都"偷"过来用（为了实现简单，这里选择一次性偷走对方全部的空闲链表，而不是只偷一页）。

> kalloc：自己没有就去邻居 CPU 那里"偷"一点内存过来

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

同样这里拿 CPU 号，和之前一样要用 `push_off()`/`pop_off()` 包裹一下

```c
push_off();
int id = cpuid();
pop_off();
```

整个过程中，任意时刻最多只持有一把 `kmem` 锁（从不同时抓两把），所以不会有死锁风险。

至此 `memory allocator` 部分已经完成。按照提示，在xv6 系统里运行 `kalloctest/usertests sbrkmuch` 结果类似下图所示，说明实现成功。

![2026-07-17 23.44.39](./2026-07-17%2023.44.39.png)

![2026-07-17 23.44.49](./2026-07-17%2023.44.49.png)

最后再检查一下 `` 看看有没有影响到原来的功能：

![2026-07-17 23.46.41](./2026-07-17%2023.46.41.png)

## read-write lock (moderate/hard)

要实现一个读写锁（改 `initrwlock`/`read_acquire`/`read_release`/`write_acquire`/`write_release` 这几个函数），要求：

- 多个 reader 可以同时持锁；
- writer 独占（有 writer 时不能有其他 reader/writer）；
- writer 不能被饿死：一旦有 writer 在等，后面来的 reader 必须排在它后面等，不能插队持续抢占。

首先在 `rwspinlock` 里加三个字段：reader 计数、是否正在写、有多少 writer 在排队等待：

```c
struct rwspinlock {
  // Replace this with your implementation.
  struct spinlock l;
  int readers;
  int writing;
  int waiting; // this means the waiting for write operation
};
```

然后就是简单的初始化,全部设置成0：

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

然后我们看请求读锁这部分，用一个 while 循环包裹检查，只有没有 writer 在等、也没有 writer 在写时才能进，那就把 reader 计数加一，否则就继续等待。

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

释放读锁就简单了，直接 reader 计数减 1，全程用底层锁保证原子性：

```c
static void
read_release_inner(struct rwspinlock *rwlk)
{
  acquire(&rwlk->l);
  rwlk->readers--;
  release(&rwlk->l);
}
```

请求写锁的逻辑需要稍微想一下，首先要"排队"挡住后来的 reader，再等现有 reader 走完.

关键逻辑：writer 一来，先把自己计入等待队列（`waiting++`），这一步会立刻挡住后续想插队的 reader；然后再等"没有人在写、也没有 reader 在读"的时候，才真正拿到写锁：

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

`waiting++` 和真正判断能否写这两步是分开的，这也是不会饿死的关键：只要有一个 writer 完成了"报到"，`waiting > 0` 就成立，reader 就进不来了，即便这个 writer 本身还要继续等已有的 reader 读完。

释放写锁，直接把正在写入改成没有人在写就好.

```c
static void
write_release_inner(struct rwspinlock *rwlk)
{
  acquire(&rwlk->l);
  rwlk->writing = 0;
  release(&rwlk->l);
}
```

完成后，通过运行 `rwlktest` 来测试你的 rwspinlock 实现。你应该会看到类似下面的输出：

![2026-07-18 00.42.05](./2026-07-18%2000.42.05.png)

## Lab 7 整体测试

完成所有练习后，执行以下命令对 Lab 7 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-locks
```

![2026-07-18 14.02.04](./2026-07-18%2014.02.04.png)