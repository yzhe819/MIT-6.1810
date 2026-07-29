# Lab 8 实验报告

**目标简述：** 为 xv6 文件系统加入二级间接块支持以实现大文件，并实现符号链接（`symlink`）及 `open()` 的跟随/不跟随语义。

**实验难度：** `large files` (hard) · `symbolic links` (moderate)

## large files (moderate)

这一部分要求给 xv6 添加大文件支持。现在的结构是：一个 inode 有 12 个 direct block number，加上 1 个 singly-indirect block number。一个 singly-indirect block 能再指向 256 个 block number，所以现在总共支持 12+256=268 个 block。

题目要求把原来的 12 个 direct block 改成 11 个，腾出来的那一个改成 doubly-indirect block：它指向 256 个 singly-indirect block，每个 singly-indirect block 再指向 256 个 block（两层间接）。这样算下来，整体最大支持大小变成 11 + 256 + 256*256 = 65803 个 block。

下面来看具体实现。根据实验提示，首先要修改的是 `kernel/fs.h` 里定义 inode 块数量的部分。为了方便后面使用，这里顺手加了一个宏直接算出最大支持大小；同时也别忘了同步修改 `dinode` 结构体，因为它的 `addrs[]` 数组大小依赖 `NDIRECT`。

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

接下来是 `kernel/fs.c` 里的 `bmap()` 函数，它负责把文件内的「逻辑块号」翻译成磁盘上的实际块号，是这次实验的核心修改对象。

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

整体思路和原来的直接块、一级间接块判断逻辑是一致的：先判断这个逻辑块号落在哪个区间。如果超出了直接块和一级间接块的范围（即 11 + 256 之外），那就说明它落在新加的二级间接块里。这时用整除得到它属于哪一个一级间接块，再用取余得到它在这个一级间接块里的具体位置。因为要经过两层间接块，所以这里需要 `bread()` 两次，对应的失败处理和 `brelse()` 释放逻辑和原来直接块/一级间接块的写法保持一致。

别忘了 itrunc, `bmap` 只负责"分配"，文件被删除或截断时，负责"释放"的是 `itrunc`。lab 提示里特别强调了这一点：**itrunc 必须能释放包括 doubly-indirect block 在内的所有 block**，否则跑 `usertests` 时会出现磁盘块泄漏（分配得出去，却永远还不回来，最后把整个文件系统写满）。在原来两层（direct + 一层 indirect）的基础上，需要再加一段，把 doubly-indirect block 指向的每一个 singly-indirect block，连同它们各自指向的数据块，都释放掉：

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

> 释放 doubly-indirect block：先逐个释放它指向的 singly-indirect block 里的数据块，再释放 singly-indirect block 本身，最后释放 doubly-indirect block 自己

完成后，通过运行 `bigfile` 来测试你的 bigfile 实现。你应该会看到类似下面的输出：

![2026-07-18 17.36.08](./2026-07-18%2017.36.08.png)

## Symbolic links

实验第二部分要求给 xv6 添加符号链接（symbolic link）这种新的文件类型。符号链接本身不存实际数据，它指向另一个目标文件；当用户打开符号链接时，内核会去查找这个目标路径，再真正打开目标文件。符号链接允许在目标文件还不存在时就被创建——只是等到真正打开它的时候，如果目标依然不存在，内核会返回错误。


先去 `kernel/stat.h` 添加这个新的文件类型：

```c
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device
#define T_SYMLINK 4   // Symbolic links
```

然后去 `kernel/syscall.h` 和 `kernel/syscall.c` 定义这个函数，把对应的 system call 安排好，跟之前添加 system call （lab2) 的流程完全一样。

在 `kernel/syscall.h` 里：

```c
#define SYS_write    16
#define SYS_mknod    17
#define SYS_unlink   18
#define SYS_link     19
#define SYS_mkdir    20
#define SYS_close    21
#define SYS_symlink  22
```

在 `kernel/syscall.c` 里：

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

最后导出给 user mode 使用，配置一下 `user/user.h` 和 `user/usys.pl`：

```c
int getpid(void);
char* sys_sbrk(int,int);
int pause(int);
int uptime(void);
int symlink(char *target, char *path);
```

还有user/usys.pl
```pl
entry("dup");
entry("getpid");
entry("sbrk");
entry("pause");
entry("uptime");
entry("symlink");
```

到这里，`symlink` 这个系统调用的「骨架」就搭好了，接下来去 `kernel/sysfile.c` 里实现真正的逻辑。

`symlink(char *target, char *path)` 有两个字符串参数，用 `argstr` 分别取出 `target` 和 `path`：

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

整体逻辑不复杂：先取出用户传入的 `target` 和 `path` 两个字符串，在 `path` 处用 `create()` 创建一个类型为 `T_SYMLINK` 的新 inode，再用 `writei()` 把 `target` 字符串（包含结尾的 `\0`）写进这个 inode 的数据块里。最后别忘了 `iunlockput` 释放这个 inode，整个操作也要用 `begin_op()`/`end_op()` 包裹起来，保证是一次完整的日志事务。

### 修改 open

创建符号链接文件的逻辑完成后，下一步是修改 `open()` 的逻辑，让它在遇到符号链接时，不再直接打开这个链接文件本身，而是「跳转」到它指向的目标，再去打开目标文件。

`open` 对应的实现 `sys_open` 也在同一个文件里。在动手改之前，需要先想清楚一种特殊情况：如果符号链接 a 指向 b，而 b 又指向回 a，这时候打开 a 会发生什么？答案是——陷入无限循环！

实验的 hint 里也专门提到了这一点：系统需要一个计数器（这里叫 `depth`）记录已经跳转了多少次符号链接，一旦超过某个上限（题目建议 10 次就足够识别出循环了），就直接返回错误，从而打破这种潜在的无限循环。

这里会用到两个关键函数：

- `namei(path)`：根据路径名做一次完整的路径解析，从根目录（或当前目录）开始逐级查找，返回对应的 inode（不加锁）。相当于把"文件名"翻译成"inode"。
- `readi(ip, user_dst, dst, off, n)`：从 `ip` 对应的文件内容里，从偏移 `off` 开始读 `n` 字节，写到 `dst`。`user_dst` 用来区分 `dst` 是用户态地址还是内核态地址——这里传 0，因为 `target` 是内核栈上的数组。


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

这里用一个 `while(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW))` 循环来处理连续跳转的情况，同时判断用户传入的 flag 里有没有 `O_NOFOLLOW`——如果有，就直接打开符号链接本身，不再跟随目标；如果没有，就在循环里不断用 `readi()` 读出下一跳的目标路径，再用 `namei()` 把 `ip` 指向这个目标对应的 inode，直到遇到一个非符号链接的文件，或者达到跳转次数上限为止。

到这里已经接近完成了，只差最后一步——把题目里面提到的 `O_NOFOLLOW` 这个 flag 定义出来。去 `kernel/fcntl.h` 里加上：

```h
#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400
#define O_NOFOLLOW 0x800
```

最后一步是在 Makefile 里把测试程序加进去，然后就可以编译运行测试了：

```c
ifeq ($(LAB),fs)
UPROGS += \
	$U/_bigfile\
	$U/_symlinktest
endif
```

完成后，通过运行 `symlinktest` 来测试你的 Symbolic links 实现。你应该会看到类似下面的输出：

![2026-07-18 21.17.20](./2026-07-18%2021.17.20.png)

## Lab 8 整体测试

完成所有练习后，执行以下命令对 Lab 8 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-fs
```

![2026-07-18 21.28.28](./2026-07-18%2021.28.28.png)