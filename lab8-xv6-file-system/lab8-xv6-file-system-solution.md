# Lab 8 Report (AI)

**Objective Summary:** Add doubly-indirect block support to the xv6 file system to implement large files, and implement symbolic links (`symlink`) as well as the follow/no-follow semantics of `open()`.

**Experiment Difficulty:** `large files` (hard) · `symbolic links` (moderate)

## large files (moderate)

This part requires adding large file support to xv6. The current structure is: an inode has 12 direct block numbers, plus 1 singly-indirect block number. A singly-indirect block can further point to 256 block numbers, so currently it supports a total of 12+256=268 blocks.

The task requires modifying the original 12 direct blocks to 11, and changing the freed up one to a doubly-indirect block: it points to 256 singly-indirect blocks, and each singly-indirect block further points to 256 blocks (two levels of indirection). Calculated this way, the overall maximum supported size becomes 11 + 256 + 256*256 = 65803 blocks.

Let's look at the specific implementation below. According to the experiment hints, the first thing to modify is the part defining the number of inode blocks in `kernel/fs.h`. For convenience of later use, a macro is added here to directly calculate the maximum supported size; at the same time, don't forget to synchronously modify the `dinode` structure, because its `addrs[]` array size depends on `NDIRECT`.

```h
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint)) // 1024/4 = 256
#define NDOUBLYINDIRECT (NINDIRECT * NINDIRECT) // 256*256 = 65536
#define MAXFILE (NDIRECT + NINDIRECT + NDOUBLYINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses
};
```

Next is the `bmap()` function in `kernel/fs.c`, which is responsible for translating the "logical block number" within a file into the actual physical block number on the disk. This is the core modification target of this experiment.

```c
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT) {
    if((addr = ip->addrs[bn]) == 0) {
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT) {
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0) {
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[NDIRECT] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0) {
      addr = balloc(ip->dev);
      if(addr) {
        a[bn] = addr;
        log_write(bp);
      }
    }
    brelse(bp);
    return addr;
  }
  bn -= NINDIRECT;

  if(bn < NINDIRECT * NINDIRECT) {
    // calculate the block number for first level
    int index = bn / NINDIRECT;
    // use the reminder to get the item number
    int rem = bn % NINDIRECT;

    // get the last one, 11+1
    if((addr = ip->addrs[NDIRECT + 1]) == 0) {
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[NDIRECT + 1] = addr;
    }

    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    if((addr = a[index]) == 0) {
      addr = balloc(ip->dev);
      if(addr == 0) {
        brelse(bp);
        return 0;
      }
      a[index] = addr;
      log_write(bp);
    }
    brelse(bp);

    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    if((addr = a[rem]) == 0) {
      addr = balloc(ip->dev);
      if(addr) {
        a[rem] = addr;
        log_write(bp);
      }
    }
    brelse(bp);

    return addr;
  }

  panic("bmap: out of range");
}
```

The overall logic is consistent with the original logic for direct blocks and singly-indirect blocks: first determine which range this logical block number falls into. If it exceeds the range of direct blocks and singly-indirect blocks (i.e., outside of 11 + 256), it indicates that it falls within the newly added doubly-indirect block. In this case, use integer division to get which singly-indirect block it belongs to, and then use the remainder to get its specific location within this singly-indirect block. Because it has to pass through two levels of indirect blocks, `bread()` is required twice here. The corresponding failure handling and `brelse()` release logic should remain consistent with the original code for direct/singly-indirect blocks.

Don't forget `itrunc`. `bmap` is only responsible for "allocation"; when a file is deleted or truncated, `itrunc` is responsible for "release". The lab hint specifically emphasizes this point: **`itrunc` must be able to free all blocks, including the doubly-indirect block**, otherwise disk block leaks will occur when running `usertests` (blocks are allocated but never returned, eventually filling up the entire file system). On top of the original two layers (direct + one layer of indirect), a new section needs to be added to free each singly-indirect block pointed to by the doubly-indirect block, along with the data blocks they respectively point to:

```c
// Truncate inode (discard contents).
// Caller must hold ip->lock.
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  struct buf *bp2;
  uint *a;
  uint *a2;

  for(i = 0; i < NDIRECT; i++) {
    if(ip->addrs[i]) {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]) {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++) {
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  if(ip->addrs[NDIRECT + 1]) {
    bp = bread(ip->dev, ip->addrs[NDIRECT + 1]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++) {
      if(a[j]) {
        bp2 = bread(ip->dev, a[j]);
        a2 = (uint*)bp2->data;
        for(int k = 0; k < NINDIRECT; k++) {
          // clear the all the leaf item
          if(a2[k])
            bfree(ip->dev, a2[k]);
        }
        brelse(bp2);
        bfree(ip->dev, a[j]);
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT + 1]);
    ip->addrs[NDIRECT + 1] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}
```

> Freeing the doubly-indirect block: first individually free the data blocks inside the singly-indirect blocks it points to, then free the singly-indirect blocks themselves, and finally free the doubly-indirect block itself.

After finishing, test your bigfile implementation by running `bigfile`. You should see output similar to the following:

![2026-07-18 17.36.08](./2026-07-18%2017.36.08.png)

## Symbolic links

The second part of the experiment requires adding a new file type, the symbolic link, to xv6. The symbolic link itself does not store actual data; it points to another target file. When a user opens a symbolic link, the kernel will look up this target path and then truly open the target file. Symbolic links allow creation even when the target file does not yet exist—only when it is actually opened, if the target still does not exist, will the kernel return an error.

First, add this new file type to `kernel/stat.h`:

```c
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device
#define T_SYMLINK 4   // Symbolic links
```

Then go to `kernel/syscall.h` and `kernel/syscall.c` to define this function and arrange the corresponding system call. This process is exactly the same as adding a system call before (lab2).

In `kernel/syscall.h`:

```c
#define SYS_write    16
#define SYS_mknod    17
#define SYS_unlink   18
#define SYS_link     19
#define SYS_mkdir    20
#define SYS_close    21
#define SYS_symlink  22
```

In `kernel/syscall.c`:

```c
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_symlink(void);

...

[SYS_unlink]    sys_unlink,
[SYS_link]      sys_link,
[SYS_mkdir]     sys_mkdir,
[SYS_close]     sys_close,
[SYS_symlink]   sys_symlink,
};
```

Finally, export it for user mode use, by configuring `user/user.h` and `user/usys.pl`:

```c
int getpid(void);
char* sys_sbrk(int,int);
int pause(int);
int uptime(void);
int symlink(char *target, char *path);
```

And in `user/usys.pl`:
```pl
entry("dup");
entry("getpid");
entry("sbrk");
entry("pause");
entry("uptime");
entry("symlink");
```

Up to here, the "skeleton" of the `symlink` system call is built. Next, go to `kernel/sysfile.c` to implement the actual logic.

`symlink(char *target, char *path)` has two string arguments. Use `argstr` to retrieve `target` and `path` respectively:

```c
uint64
sys_symlink(void)
{
  char target[MAXPATH], path[MAXPATH];
  struct inode* ip;

  if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
    return -1;

  begin_op();

  // create a new inode
  if((ip = create(path, T_SYMLINK, 0, 0)) == 0) {
    end_op();
    return -1;
  }

  if(writei(ip, 0, (uint64)target, 0, strlen(target) + 1) !=
     strlen(target) + 1) {
    iunlockput(ip);
    end_op();
    return -1;
  }

  iunlockput(ip);
  end_op();

  return 0;
}
```

The overall logic is not complicated: first retrieve the two strings `target` and `path` passed by the user, use `create()` to create a new inode of type `T_SYMLINK` at `path`, and then use `writei()` to write the `target` string (including the trailing `\0`) into the data block of this inode. Finally, don't forget to use `iunlockput` to release this inode. The entire operation also needs to be wrapped with `begin_op()`/`end_op()` to ensure it is a complete log transaction.

### Modifying open

After finishing the logic for creating symbolic link files, the next step is to modify the logic of `open()`, so that when it encounters a symbolic link, it no longer directly opens the link file itself, but "jumps" to the target it points to, and then opens the target file.

The implementation `sys_open` corresponding to `open` is also in the same file. Before making changes, you need to first think about a special case: what happens if a symbolic link a points to b, and b points back to a? The answer is—falling into an infinite loop!

The experiment's hint also specifically mentions this point: the system needs a counter (called `depth` here) to record how many symbolic link jumps have been made. Once a certain upper limit is exceeded (the problem suggests 10 times is enough to identify a loop), it directly returns an error, thereby breaking this potential infinite loop.

Two key functions will be used here:

- `namei(path)`: Performs a complete path resolution based on the path name, searching level by level from the root directory (or current directory), and returns the corresponding inode (unlocked). Equivalent to translating the "file name" into an "inode".
- `readi(ip, user_dst, dst, off, n)`: Reads `n` bytes from the file content corresponding to `ip`, starting from the offset `off`, and writes to `dst`. `user_dst` is used to distinguish whether `dst` is a user-space address or a kernel-space address—pass 0 here because `target` is an array on the kernel stack.


```c
uint64
sys_open(void)
{
  char path[MAXPATH];
  char target[MAXPATH];
  int fd, omode;
  struct file* f;
  struct inode* ip;
  int n;
  int depth = 0;

  argint(1, &omode);
  if((n = argstr(0, path, MAXPATH)) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE) {
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0) {
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0) {
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY) {
      iunlockput(ip);
      end_op();
      return -1;
    }

    // symlink file handler
    while(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)) {
      depth++;

      if(depth > 10) {
        // achieve the system link depth limitation
        iunlockput(ip);
        end_op();
        return -1;
      }

      if(ip->size > MAXPATH)
        return -1;
      
      if(readi(ip, 0, (uint64)target, 0, ip->size) != ip->size) {
        iunlockput(ip);
        end_op();
        return -1;
      }
      iunlockput(ip);

      if((ip = namei(target)) == 0) {
        end_op();
        return -1;
      }

      // add the lock for ip at the end
      ilock(ip);
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)) {
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE) {
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE) {
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}
```

Here, a `while(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW))` loop is used to handle continuous jumps, and simultaneously check whether the flag passed by the user contains `O_NOFOLLOW`—if it does, it directly opens the symbolic link itself and no longer follows the target; if not, it continuously uses `readi()` within the loop to read the target path of the next jump, and then uses `namei()` to point `ip` to the inode corresponding to this target, until a non-symbolic link file is encountered or the jump count upper limit is reached.

We are almost done here, with only the last step remaining—defining the `O_NOFOLLOW` flag mentioned in the problem. Add it to `kernel/fcntl.h`:

```h
#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400
#define O_NOFOLLOW 0x800
```

The last step is to add the test programs in the Makefile, and then you can compile and run the tests:

```c
ifeq ($(LAB),fs)
UPROGS += \
	$U/_bigfile\
	$U/_symlinktest
endif
```

After finishing, test your Symbolic links implementation by running `symlinktest`. You should see output similar to the following:

![2026-07-18 21.17.20](./2026-07-18%2021.17.20.png)

## Lab 8 Comprehensive Test

After completing all exercises, execute the following command to run a complete test on Lab 8 to verify the correctness of all functional implementations:

```sh
./grade-lab-fs
```

![2026-07-18 21.28.28](./2026-07-18%2021.28.28.png)
