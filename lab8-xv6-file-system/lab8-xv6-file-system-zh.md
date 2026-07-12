# Lab 8：文件系统（File System）

[English](./lab8-xv6-file-system.md) · [中文](./lab8-xv6-file-system-zh.md)

本实验需要你在 xv6 文件系统中实现两项能力：**大文件支持**与**符号链接（symlink）**。

开始编码前，请阅读 [xv6 手册](xv6/book-riscv-rev5.pdf) 的 **第 10 章：File system**，并结合对应源码理解实现。

获取实验代码并切换分支：

```bash
$ git fetch
$ git checkout fs
$ make clean
```

---

## 大文件（hard）

未修改的 xv6 中文件最大为 **268 个块**（`12` 个直接块 + `1` 个一级间接块中的 `256` 个数据块地址）。

`bigfile` 可观察这个限制：

```text
$ bigfile
..
wrote 268 blocks
bigfile: file is too small
$
```

而本实验要求支持到 **65803 个块**。

### 目标

在 inode 中加入**二级间接块（doubly-indirect）**，布局改为：

- 11 个直接块
- 1 个一级间接块
- 1 个二级间接块

最大块数变为：

- `11 + 256 + 256*256 = 65803`

（因为必须牺牲一个直接块条目，以保持磁盘 inode 大小不变。）

### 预备知识

`mkfs` 负责构建文件系统镜像；大小由 `kernel/param.h` 中 `FSSIZE` 控制（本实验设为 `200000`）。构建输出应包含类似：

```text
nmeta 71 (boot, super, log blocks 31, inode blocks 13, bitmap blocks 25) blocks 199929 total 200000
```

`make qemu` 会重建 `fs.img`（旧镜像备份为 `fs.img.bk`）。若要复用现有镜像，可用 `make qemu-fs`。

### 建议先看哪里

- `fs.h` 的 `struct dinode`（重点 `NDIRECT`、`NINDIRECT`、`MAXFILE`、`addrs[]`）
- `fs.c` 的 `bmap()`（逻辑块号到磁盘块号映射）

`bmap()` 同时服务读写路径；写路径会按需分配数据块与间接块。

### 你的任务

修改 `bmap()`，在不改变磁盘 inode 大小前提下支持二级间接块。

完成标准：

```text
$ bigfile
...
wrote 65803 blocks
done; ok
$ usertests -q
...
ALL TESTS PASSED
$
```

`bigfile` 至少需要约 1 分 30 秒。

### 提示

- 先彻底理解 `bmap()`，并画清 `ip->addrs[]`、一级/二级间接块与数据块关系图。
- 明确逻辑块号如何索引到二级间接层和一级间接层。
- 若修改 `NDIRECT`，要同步检查 `file.h` 中 `struct inode` 的 `addrs[]`，保证内存/磁盘 inode 布局元素数量一致。
- 改完 `NDIRECT` 后要重建 `fs.img`（mkfs 依赖它）。
- 若文件系统状态损坏（例如崩溃后），在宿主机删除 `fs.img` 再重建。
- 每次 `bread()` 后别忘了 `brelse()`。
- 间接块与二级间接块应按需分配，保持懒分配策略。
- 修改 `itrunc()`，确保释放文件的全部块（包括二级间接体系）。
- 本实验 `FSSIZE` 更大、文件更大，`usertests` 会比之前更慢。

---

## 符号链接（moderate）

本部分要求你为 xv6 增加符号链接（软链接）。

符号链接保存的是**目标路径名**。打开符号链接时，内核要继续解析该目标路径。它与硬链接不同：硬链接是对具体 inode 的链接，而符号链接是按路径名解析到“当前该名字指向的对象”。

本实验范围：

- 不要求专门处理“指向目录的符号链接”。
- 只需让 `open()` 具备跟随符号链接能力。

### 你的任务

实现系统调用：

```c
int symlink(char *target, char *path);
```

它在 `path` 创建一个符号链接，目标为 `target`。

把 `symlinktest` 加入 `Makefile` 后，预期输出：

```text
$ symlinktest
Start: test symlinks
test symlinks: ok
Start: test concurrent symlinks
test concurrent symlinks: ok
$ usertests -q
...
ALL TESTS PASSED
$
```

### 提示

- 先补齐 syscall 编号与用户/内核侧声明与桩代码（`user/usys.pl`、`user/user.h`、`kernel/sysfile.c` 等）。
- 在 `kernel/stat.h` 新增文件类型 `T_SYMLINK`。
- 在 `kernel/fcntl.h` 新增 `O_NOFOLLOW`（位标志不可与现有标志冲突）。
- `sys_symlink` 中，即使 `target` 当前不存在，也应允许创建成功。
- 可把目标路径保存到 symlink inode 的数据块中（这是一种可行实现）。
- `symlink` 成功返回 `0`，失败返回 `-1`（与 `link`/`unlink` 风格一致）。
- 修改 `open()`：
  - 路径不存在则失败
  - 指定 `O_NOFOLLOW` 时打开 symlink 本身，不跟随
  - 否则递归跟随直到遇到非 symlink
  - 处理循环链接（可用深度阈值近似，如 10）
- 其他系统调用（如 `link`、`unlink`）不应跟随 symlink，应作用于 symlink 本身。

---

## 提交实验

### 记录耗时

创建 `time.txt`，写入一个整数（实验耗时小时数），然后 `git add` + `git commit`。

### 回答问题

若实验包含问答题，将答案写到 `answers-*.txt`，然后提交。

### 提交方式

作业通过 Gradescope 提交。你需要 MIT Gradescope 账号；加入码见 Piazza。如需帮助可参考[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

运行 `make zipball` 生成 `lab.zip` 后上传。

若存在未提交或未跟踪文件，可能出现：

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

确保实验所需文件都已被跟踪（不在 `??` 行里），必要时 `git add {filename}`。

- 提交前运行 `make grade`
- `make zipball` 前先提交源码改动
- Gradescope 实验分数即最终实验成绩

---

## 可选挑战

支持三级间接块（triple-indirect blocks）。

### 致谢

感谢 UW CSEP551（2019 秋）课程团队提供 symlink 练习思路。

---

*如有关于 6.1810 的问题或建议，请邮件联系课程组：61810-staff@lists.csail.mit.edu。*
