# Lab 8: File System

[English](./lab8-xv6-file-system.md) · [中文](./lab8-xv6-file-system-zh.md)

In this lab you will add **large-file support** and **symbolic links** to the xv6 file system.

Before coding, read **Chapter 10: File system** in the [xv6 book](xv6/book-riscv-rev5.pdf) and review the related source code.

Fetch the lab source and switch branch:

```bash
$ git fetch
$ git checkout fs
$ make clean
```

---

## Large Files

In unmodified xv6, file size is limited to **268 blocks** (`12` direct + `256` via one singly-indirect block).

`bigfile` demonstrates the current limit:

```text
$ bigfile
..
wrote 268 blocks
bigfile: file is too small
$
```

But this lab expects files up to **65803 blocks**.

### Goal

Add a **doubly-indirect block** to each inode:

- 11 direct blocks
- 1 singly-indirect block
- 1 doubly-indirect block

Maximum file blocks become:

- `11 + 256 + 256*256 = 65803`

(You sacrifice one direct entry to keep on-disk inode size unchanged.)

### Preliminaries

`mkfs` builds the file-system image; size is controlled by `FSSIZE` in `kernel/param.h` (set to `200000` in this lab). In build output, you should see a line like:

```text
nmeta 71 (boot, super, log blocks 31, inode blocks 13, bitmap blocks 25) blocks 199929 total 200000
```

`make qemu` creates a new `fs.img` (old one saved as `fs.img.bk`). To reuse existing image, run `make qemu-fs`.

### What to Inspect

- `struct dinode` in `fs.h` (`NDIRECT`, `NINDIRECT`, `MAXFILE`, `addrs[]`)
- `bmap()` in `fs.c` (maps logical block numbers to disk block numbers)

`bmap()` is used for both reads and writes; writes allocate data/indirect blocks on demand.

### Your Job

Modify `bmap()` to support doubly-indirect blocks while keeping inode size unchanged.

Done criteria:

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

`bigfile` takes at least ~1.5 minutes.

### Hints

- Fully understand `bmap()` and draw the block-addressing hierarchy.
- Work out how logical block number indexes doubly-indirect + singly-indirect levels.
- If you change `NDIRECT`, update related `addrs[]` declarations (e.g., `struct inode` in `file.h`) so in-memory/on-disk inode layouts stay consistent.
- Rebuild `fs.img` after changing `NDIRECT` (mkfs depends on it).
- If file system gets corrupted, delete `fs.img` in Unix host and rebuild.
- `brelse()` every block returned by `bread()`.
- Allocate indirect/doubly-indirect blocks lazily, only when needed.
- Update `itrunc()` to free all blocks, including doubly-indirect hierarchy.
- `usertests` runs longer in this lab due to larger `FSSIZE` and bigger files.

---

## Symbolic Links

Implement symbolic links (soft links) in xv6.

Symbolic links store a **pathname target**. On open, kernel resolves the target path. Unlike hard links, symlinks can refer by name and can point across different contexts in principle.

For this lab:

- You **do not** need to handle symlinks to directories specially.
- Only `open()` needs symlink-following logic.

### Your Job

Implement:

```c
int symlink(char *target, char *path);
```

It should create a symlink at `path` that refers to `target`.

After adding `symlinktest` to `Makefile`, expected output:

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

### Hints

- Add syscall number + user/kernel plumbing (`user/usys.pl`, `user/user.h`, `kernel/sysfile.c`, etc.).
- Add file type `T_SYMLINK` in `kernel/stat.h`.
- Add `O_NOFOLLOW` in `kernel/fcntl.h` (must not overlap existing flags).
- In `sys_symlink`, creating symlink should succeed even if `target` does not currently exist.
- Store target pathname in symlink inode data (one valid approach).
- `symlink` returns `0` on success, `-1` on failure (like `link`/`unlink` style).
- Modify `open()`:
  - if path missing: fail
  - if `O_NOFOLLOW`: open symlink itself
  - otherwise recursively follow links until non-symlink
  - detect cycles (e.g., depth limit such as 10)
- Other syscalls (e.g., `link`, `unlink`) should operate on symlink itself and **must not** follow it.

---

## Submit the Lab

### Time Spent

Create `time.txt` with one integer (hours spent), then `git add` and `git commit`.

### Answers

If this lab has questions, write answers in `answers-*.txt`, then `git add` and `git commit`.

### Submit

Submission uses Gradescope. You need an MIT Gradescope account; see Piazza for class entry code. If needed, use [this help link](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code).

Run `make zipball` to generate `lab.zip`, then upload it.

If there are uncommitted or untracked files, output may include:

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

Ensure required files are tracked (not `??`). Use `git add {filename}`.

- Run `make grade` before submission.
- Commit modified source before running `make zipball`.
- Gradescope lab grade is your final lab grade.

---

## Optional Challenge Exercises

Support triple-indirect blocks.

### Acknowledgment

Thanks to the staff of UW CSEP551 (Fall 2019) for the symlink exercise.

---

*Questions or comments regarding 6.1810? Send e-mail to the course staff at 61810-staff@lists.csail.mit.edu.*
