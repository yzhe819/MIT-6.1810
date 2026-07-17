// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

#ifdef LAB_LOCK
#define NLOCK 500

static struct spinlock *locks[NLOCK];
struct spinlock lock_locks;

void
freelock(struct spinlock *lk)
{
  acquire(&lock_locks);
  int i;
  for(i = 0; i < NLOCK; i++) {
    if(locks[i] == lk) {
      locks[i] = 0;
      break;
    }
  }
  release(&lock_locks);
}

static void
findslot(struct spinlock *lk)
{
  acquire(&lock_locks);
  int i;
  for(i = 0; i < NLOCK; i++) {
    if(locks[i] == 0) {
      locks[i] = lk;
      release(&lock_locks);
      return;
    }
  }
  panic("findslot");
}
#endif

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
#ifdef LAB_LOCK
  lk->nts = 0;
  lk->n = 0;
  findslot(lk);
#endif
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

#ifdef LAB_LOCK
  __sync_fetch_and_add(&(lk->n), 1);
#endif

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0) {
#ifdef LAB_LOCK
    __sync_fetch_and_add(&(lk->nts), 1);
#else
    ;
#endif
  }

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

#ifdef LAB_LOCK
static void
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

static void
read_release_inner(struct rwspinlock *rwlk)
{
  acquire(&rwlk->l);
  rwlk->readers--;
  release(&rwlk->l);
}

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

static void
write_release_inner(struct rwspinlock *rwlk)
{
  acquire(&rwlk->l);
  rwlk->writing = 0;
  release(&rwlk->l);
}

void
read_acquire(struct rwspinlock *rwlk)
{
  push_off(); // disable interrupts to avoid deadlock.
  read_acquire_inner(rwlk);
}

void
read_release(struct rwspinlock *rwlk)
{
  read_release_inner(rwlk);
  pop_off();
}

void
write_acquire(struct rwspinlock *rwlk)
{
  push_off(); // disable interrupts to avoid deadlock.
  write_acquire_inner(rwlk);
}

void
write_release(struct rwspinlock *rwlk)
{
  write_release_inner(rwlk);
  pop_off();
}

void
initrwlock(struct rwspinlock *rwlk)
{
  // Replace this with your implementation.
  initlock(&rwlk->l, "rwlk");
  rwlk->readers = 0;
  rwlk->writing = 0;
  rwlk->waiting = 0;
}

// Test rwspinlock implementation.
static void
rwspinlock_test_step(uint step, const char *msg)
{
  static uint barrier;
  const uint ncpu = 4;

  __atomic_fetch_add(&barrier, 1, __ATOMIC_ACQ_REL);
  while(__atomic_load_n(&barrier, __ATOMIC_RELAXED) < ncpu * step) {
    // spin
  }

  if(cpuid() == 0) {
    printf("rwspinlock_test: step %d: %s\n", step, msg);
  }
}

static uint
delay()
{
  static uint v;
  for(int i = 0; i < 10000; i++) {
    __atomic_fetch_add(&v, 1, __ATOMIC_RELAXED);
  }
  return __atomic_load_n(&v, __ATOMIC_RELAXED);
}

uint64
sys_rwlktest()
{
  int r = 0;
  int step = 0;

  push_off();
  int id = cpuid();

  rwspinlock_test_step(++step, "initrwlock");

  static struct rwspinlock l;
  if(id == 0) {
    initrwlock(&l);
  }

  rwspinlock_test_step(++step, "concurrent read_acquire");

  for(int i = 0; i < 1000000; i++)
    read_acquire(&l);

  rwspinlock_test_step(++step, "concurrent read_release");

  for(int i = 0; i < 1000000; i++)
    read_release(&l);

  rwspinlock_test_step(++step, "prepare read_acquire for writer priority test");

  if(id == 1) {
    for(int i = 0; i < 30; i++) {
      read_acquire(&l);
    }
  }

  rwspinlock_test_step(++step, "writer priority test");

  static uint flag;
  if(id == 0) {
    write_acquire(&l);
    __atomic_store_n(&flag, 1, __ATOMIC_RELAXED);
    write_release(&l);
  }

  if(id == 1) {
    delay();
    for(int i = 0; i < 10; i++) {
      read_release(&l);
    }
    delay();
    for(int i = 0; i < 10; i++) {
      read_release(&l);
    }
    delay();
    for(int i = 0; i < 10; i++) {
      read_release(&l);
    }
  }

  if(id == 2) {
    delay();
    read_acquire(&l);
    uint f = __atomic_load_n(&flag, __ATOMIC_RELAXED);
    if(f == 0) {
      printf("rwspinlock_test: reader sneaked ahead of waiting writer\n");
      r = -1;
    }
    read_release(&l);
  }

  rwspinlock_test_step(++step, "checking for concurrent readers/writers");

  static uint v;
  if(id == 0) {
    uint maxwv = 0;
    for(int i = 0; i < 1000000; i++) {
      write_acquire(&l);
      uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
      if(x > maxwv) {
        maxwv = x;
      }
      uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL);
      if(y > maxwv) {
        maxwv = y;
      }
      write_release(&l);
    }
    if(maxwv > 1) {
      printf("rwspinlock_test: cpu %d saw concurrent reads/writes: %d\n", id,
             maxwv);
      r = -1;
    }
  } else {
    uint maxrv = 0;
    for(int i = 0; i < 1000000; i++) {
      read_acquire(&l);
      uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
      if(x > maxrv) {
        maxrv = x;
      }
      uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL);
      if(y > maxrv) {
        maxrv = y;
      }
      read_release(&l);
    }
    if(maxrv < 2) {
      printf("rwspinlock_test: cpu %d never saw concurrent reads: %d\n", id,
             maxrv);
      r = -1;
    }
  }

  rwspinlock_test_step(++step, "checking for concurrent writers");

  uint maxwv = 0;
  for(int i = 0; i < 1000000; i++) {
    write_acquire(&l);
    uint x = __atomic_add_fetch(&v, 1, __ATOMIC_ACQ_REL);
    if(x > maxwv) {
      maxwv = x;
    }
    uint y = __atomic_fetch_sub(&v, 1, __ATOMIC_ACQ_REL);
    if(y > maxwv) {
      maxwv = y;
    }
    write_release(&l);
  }
  if(maxwv > 1) {
    printf("rwspinlock_test: cpu %d saw concurrent writes: %d\n", id, maxwv);
    r = -1;
  }

  rwspinlock_test_step(++step, "acquiring multiple locks");

  struct rwspinlock l2;
  initrwlock(&l2);
  write_acquire(&l2);
  read_acquire(&l);

  rwspinlock_test_step(++step, "releasing multiple locks");

  write_release(&l2);
  read_release(&l);

  for(int i = 0; i < 10; i++) {
    rwspinlock_test_step(
        ++step, "prepare read_acquire for multiple writer priority test");

    static uint writer_count;
    if(id == 3) {
      writer_count = 0;
      read_acquire(&l);
      read_acquire(&l);
    }

    rwspinlock_test_step(++step, "multiple writer priority test");

    if(id == 0 || id == 1) {
      write_acquire(&l);
      writer_count++;
      delay();
      write_release(&l);
    }

    if(id == 2) {
      delay();
      read_acquire(&l);
      if(writer_count == 0) {
        printf(
            "rwspinlock_test: reader sneaked ahead of both waiting writers\n");
        r = -1;
      }
      delay();
      delay();
      delay();
      read_release(&l);
    }

    if(id == 3) {
      delay();
      read_release(&l);
      delay();
      read_release(&l);

      delay();
      delay();

      // By this point, either one writer executed and CPU 2 is holding read
      // lock, or both writers executed.  Should never sneak ahead of second
      // writer.
      read_acquire(&l);
      if(writer_count != 2) {
        printf(
            "rwspinlock_test: reader sneaked ahead of second waiting writer\n");
        r = -1;
      }
      read_release(&l);
    }
  }

  rwspinlock_test_step(++step, "done");

  printf("rwspinlock_test(%d): %d\n", id, r);
  pop_off();

  return r;
}
#endif

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  int old = intr_get();

  // disable interrupts to prevent an involuntary context
  // switch while using mycpu().
  intr_off();

  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}

// Read a shared 32-bit value without holding a lock
int
atomic_read4(int *addr)
{
  uint32 val;
  __atomic_load(addr, &val, __ATOMIC_SEQ_CST);
  return val;
}

#ifdef LAB_LOCK
int
snprint_lock(char *buf, int sz, struct spinlock *lk)
{
  int n = 0;
  if(lk->n > 0) {
    n = snprintf(buf, sz, "lock: %s: #test-and-set %d #acquire() %d\n",
                 lk->name, lk->nts, lk->n);
  }
  return n;
}

int
statslock(char *buf, int sz)
{
  int n;
  int tot = 0;

  acquire(&lock_locks);
  n = snprintf(buf, sz, "--- lock kmem stats\n");
  for(int i = 0; i < NLOCK; i++) {
    if(locks[i] == 0)
      break;
    if(strncmp(locks[i]->name, "kmem", strlen("kmem")) == 0) {
      tot += locks[i]->nts;
      n += snprint_lock(buf + n, sz - n, locks[i]);
    }
  }

  n += snprintf(buf + n, sz - n, "--- top 5 contended locks:\n");
  int last = 100000000;
  // stupid way to compute top 5 contended locks
  for(int t = 0; t < 5; t++) {
    int top = 0;
    for(int i = 0; i < NLOCK; i++) {
      if(locks[i] == 0)
        break;
      if(locks[i]->nts > locks[top]->nts && locks[i]->nts < last) {
        top = i;
      }
    }
    n += snprint_lock(buf + n, sz - n, locks[top]);
    last = locks[top]->nts;
  }
  n += snprintf(buf + n, sz - n, "tot= %d\n", tot);
  release(&lock_locks);
  return n;
}
#endif
