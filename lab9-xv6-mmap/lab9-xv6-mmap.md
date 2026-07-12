# Lab 9: mmap

[English](./lab9-xv6-mmap.md) · [中文](./lab9-xv6-mmap-zh.md)

In this lab you will add `mmap` and `munmap` to xv6, focusing on **memory-mapped files**.

`mmap`/`munmap` are key UNIX primitives for address-space control: shared memory between processes, mapping files into memory, and user-level page-fault-driven schemes.

Fetch lab source and switch branch:

```bash
$ git fetch
$ git checkout mmap
$ make clean
```

---

## mmap API (subset for this lab, hard)

```c
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
```

For this lab, only a subset is required:

- `addr` is always `0` (kernel chooses virtual address)
- return mapped address on success, `0xffffffffffffffff` on failure
- `len` is bytes to map (may differ from file length)
- `prot` is `PROT_READ`, `PROT_WRITE`, or both
- `flags` is either `MAP_SHARED` or `MAP_PRIVATE`
- `fd` is open descriptor of file to map
- `offset` is always `0`

### Required lazy behavior

`mmap()` must be **lazy**:

- Do **not** allocate physical memory in `mmap()`
- Do **not** read file data in `mmap()`
- Allocate/map/read lazily on page fault (in/through `usertrap`)

This makes large mappings fast and supports mapping files larger than RAM.

It is acceptable that different processes mapping the same `MAP_SHARED` file do **not** share physical pages.

---

## munmap API (hard)

```c
int munmap(void *addr, size_t len);
```

`munmap()` should remove mappings in the given range.

If mapped as `MAP_SHARED` and modified, write data back to file before unmapping.

For this lab, `munmap` may unmap:

- start part of a region, or
- end part of a region, or
- the whole region

No need to support punching a hole in the middle.

On process exit, `MAP_SHARED` modifications must also be written back (as if `munmap` were called).

---

## Implementation target (VMA + page-fault path, hard)

Implement enough functionality for:

- `mmaptest` all pass
- `usertests -q` still pass

Expected end-state (similar output):

```text
$ mmaptest
test basic mmap
test basic mmap: OK
...
test fork: OK
test munmap prevents access: OK
test writes to read-only mapped memory: OK
mmaptest: all tests succeeded
$ usertests -q
...
ALL TESTS PASSED
$
```

---

## Suggested steps and hints

1. Add `_mmaptest` to `UPROGS`; add `mmap`/`munmap` syscalls so `user/mmaptest.c` compiles. Initially return errors.
2. Track per-process mappings using VMA (virtual memory area) metadata:
   - address, length, permission, flags, mapped file, etc.
   - fixed-size VMA array is acceptable (e.g., 16 entries).
3. Implement `mmap()`:
   - find unused user VA range
   - create VMA record
   - hold `struct file*` in VMA and increment refcount (e.g., `filedup`).
4. In page-fault handling for mapped region:
   - allocate physical page
   - read relevant 4096-byte file chunk using `readi` at correct offset
   - map page with correct permissions
   - lock/unlock inode appropriately for `readi`.
5. Implement `munmap()`:
   - find matching VMA / range
   - unmap pages (`uvmunmap`)
   - for `MAP_SHARED`, write mapped pages back to file
   - if entire mapping removed, decrement mapped file refcount.
6. Dirty-page optimization:
   - ideally write back only dirty pages (PTE D bit)
   - `mmaptest` does not require strict D-bit filtering; writing back without D check is acceptable.
7. Update `exit()` to unmap all mapped VMAs and flush shared modifications.
8. Update `fork()` so child inherits VMAs and file references:
   - increment file refcount for inherited VMA files
   - child may fault in its own physical pages (no need to share pages with parent).

Run `usertests -q` to confirm no regressions.

---

## Submit the Lab

### Time Spent

Create `time.txt` with one integer (hours spent), then `git add` and `git commit`.

### Answers

If this lab has questions, write answers in `answers-*.txt`, then `git add` and `git commit`.

### Submit

Submissions are via Gradescope. You need an MIT Gradescope account; see Piazza for class entry code. If needed, use [this help link](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code).

Run `make zipball` to produce `lab.zip`, then upload it.

If there are uncommitted/untracked files, output may look like:

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

Ensure required files are tracked (`git add {filename}`).

- Run `make grade` before submission.
- Commit source changes before `make zipball`.
- Gradescope lab grade is the final lab grade.

---

## Optional challenges

- Share physical pages when two processes map the same file (e.g., fork tests); requires page refcounts.
- Remove double caching by reusing buffer-cache-backed physical memory for mmap pages (set `BSIZE=4096`, pin blocks, manage refs). This also helps read/write consistency with mmap. See paper on [unified buffer cache](https://www.usenix.org/legacy/publications/library/proceedings/usenix2000/freenix/full_papers/silvers/silvers.pdf).
- Remove redundancy between lazy allocation and mmap code paths (hint: use VMA for lazy area).
- Modify `exec` to use VMAs for binary sections (on-demand paged executables).
- Implement page-out/page-in under memory pressure.

---

*Questions or comments regarding 6.1810? Send e-mail to the course staff at 61810-staff@lists.csail.mit.edu.*
