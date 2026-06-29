# Lab 1 Report

[English](./lab1-xv6-unix-utilities-solution.md) · [中文](./lab1-xv6-unix-utilities-solution-zh.md)

**Overview:** Implement a set of user-space programs and Unix utilities to become familiar with the xv6 development environment and basic usage of system calls.

**Difficulty:** `sleep` (easy) · `sixfive` (moderate) · `memdump` (easy) · `find` (moderate) · `exec` (moderate)

## Boot xv6 (easy)

Follow the lab tool setup page to configure the development environment. On macOS, install the required toolchain via Homebrew. If a security prompt appears during installation, trust the program in System Settings and re-run the command.



## sleep (easy)

This exercise requires implementing the `sleep` command in user space.

**Argument Handling**

Refer to `user/echo.c` to understand xv6's command-line argument convention: `argc` holds the number of arguments, and `argv` is a pointer to the array of argument strings. `echo` uses this mechanism to print user input back to the screen.

```c
// user/echo.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;

  for(i = 1; i < argc; i++){
    write(1, argv[i], strlen(argv[i]));
    if(i + 1 < argc){
      write(1, " ", 1);
    } else {
      write(1, "\n", 1);
    }
  }
  exit(0);
}
```

**Implementation**

1. Argument validation: exit with an error message if the argument count is not exactly 2.
2. System call: `user/user.h` exposes `int pause(int)`, which is used to implement the sleep behaviour.
3. Type conversion: command-line arguments arrive as strings, so `atoi` from `user/ulib.c` is needed to convert the tick count to an integer.

Create `user/sleep.c` as follows:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// review the user/echo.c first, understand how to get the params
int
main(int argc, char *argv[])
{
  // params handle
  if(argc != 2){
    fprintf(2, "Usage: sleep <ticks>\n");
    exit(1);
  }
  // convert char to int -> check user/ulib.c
  // use system call to pause -> check user/user.h
  pause(atoi(argv[1]));
  exit(0);
}

```

Finally, add `_sleep` to the `UPROGS` list in `Makefile` and recompile.



## sixfive (moderate)

This exercise requires scanning input files character by character, extracting valid integers, and printing only those divisible by **5 or 6**. A number ends when a separator is encountered; if a non-digit, non-separator character is seen while a number is being read (e.g. the `x` in `xv6`), the partially accumulated value is discarded.

**Referencing `cat`'s File Reading Structure**

Reading a file and processing it byte by byte closely mirrors the core logic of the `cat` command. Refer to `user/cat.c`:

```c
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];

void
cat(int fd)
{
  int n;

  while((n = read(fd, buf, sizeof(buf))) > 0) {
    if (write(1, buf, n) != n) {
      fprintf(2, "cat: write error\n");
      exit(1);
    }
  }
  if(n < 0){
    fprintf(2, "cat: read error\n");
    exit(1);
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    cat(0);
    exit(0);
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      fprintf(2, "cat: cannot open %s\n", argv[i]);
      exit(1);
    }
    cat(fd);
    close(fd);
  }
  exit(0);
}
```

`sixfive` reuses the same file-reading skeleton as `cat`; the only difference is that bytes are parsed for numeric values rather than written directly to stdout.

**Implementation**

Two state variables are introduced:
- `in_num`: tracks whether a number is currently being read.
- `num`: accumulates the current numeric value digit by digit via `num = num * 10 + (c - '0')`.

Separator detection uses the `strchr` function.

```c
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"

char buf[512];
char separators[] = " -\r\t\n./,";

void
sixfive(int fd) {
  int n;
  int in_num = 0;
  int num = 0;

  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (int i = 0; i < n; i++) {
      char c = buf[i];

      // handling number
      if (c >= '0' && c <= '9') {
        in_num = 1;
        num = num * 10 + (c - '0');
      } else if (strchr(separators, c)) {
        if (in_num) {
          if (num % 5 == 0 || num % 6 == 0) {
            fprintf(1, "%d\n", num);
          }
          // reset state
          in_num = 0;
          num = 0;
        }
      } else {
        // for example, xv6
        // we will do this reset when we detect x or v
        if (in_num) {
          in_num = 0;
          num = 0;
        }
      }
    }
  }

  // already read the entire line
  // print the matched number
  if (in_num) {
    if (num % 5 == 0 || num % 6 == 0)
      printf("%d\n", num);
  }

  if (n < 0) {
    fprintf(2, "sixfive: read error\n");
    exit(1);
  }
}

// check the user/cat.c first
int
main(int argc, char *argv[]) {
  int fd, i;

  if (argc < 2) {
    fprintf(2, "Usage: sixfive <files>\n");
    exit(1);
  }

  for (i = 1; i < argc; i++) {
    if ((fd = open(argv[i], O_RDONLY)) < 0) {
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      exit(1);
    }
    sixfive(fd);
    close(fd);
  }
  exit(0);
}

```

Add `_sixfive` to `UPROGS` in `Makefile` and run the tests after recompiling to verify correctness.



## memdump (easy)

No new file is needed for this exercise. The `void memdump(char *fmt, char *data)` function is implemented directly inside the existing `user/memdump.c`.

**Implementation**

`fmt` is a format string where each character specifies the type of a single read operation; `data` is the raw byte stream to be parsed. Casting `data` to the appropriate pointer type (e.g. `(int *)`, `(long *)`) lets the CPU load the value at the correct width in a single instruction, avoiding the error-prone approach of reading byte by byte and reconstructing via bit shifts. After each read, `data` is advanced by the corresponding number of bytes.

```c
void 
memdump(char *fmt, char *data) {

  for (int i = 0; fmt[i] != '\0'; i++) {
    char f = fmt[i];

    if (f == 'i') {
      // print the next 4 bytes of the data as a 32-bit integer, in decimal
      // use (int *) to read 4 bytes, this limit the read size
      int *p = (int *)data;
      printf("%d\n", *p);
      data += 4;
    } else if (f == 'p') {
      // print the next 8 bytes of the data as a 64-bit integer, in hex
      // long 8 bytes, and print in hex
      long *p = (long *)data;
      printf("%lx\n", *p);
      data += 8;
    } else if (f == 'h') {
      // print the next 2 bytes of the data as a 16-bit integer, in decimal
      short *p = (short *)data;
      printf("%d\n", (int)*p);
      data += 2;
    } else if (f == 'c') {
      // print the next 1 byte of the data as an 8-bit ASCII character
      printf("%c\n", *data);
      data += 1;
    } else if (f == 's') {
      // the next 8 bytes of the data contain a 64-bit pointer to a C string; print the string
      char **p = (char **)data;
      printf("%s\n", *p);
      data += 8;
    } else if (f == 'S') {
      // the rest of the data contains the bytes of a null-terminated C string; print the string
      printf("%s\n", data);
    }
  }
}

```



## find (moderate) & exec (moderate)

**Referencing `ls.c`'s Directory Traversal Structure**

`user/ls.c` demonstrates the standard approach to directory traversal in xv6: open a path with `open`, retrieve file metadata with `fstat`, and branch on file type using a `switch` statement.

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  buf[sizeof(buf)-1] = '\0';
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_DEVICE:
  case T_FILE:
    printf("%s %d %d %d\n", fmtname(path), st.type, st.ino, (int) st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, (int) st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit(0);
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
```

**Implementation**

`find` traverses the entire file tree using recursive depth-first search (DFS). To prevent infinite recursion, the special directory entries `.` and `..` are pruned and never recursed into.

Filename matching is performed by extracting the trailing component with `basename` and comparing it to the target using `strcmp`, independent of the full path.

The `-exec` feature is abstracted into a dedicated `invoke` function, which executes the specified subcommand using the standard Unix `fork` + `exec` + `wait` pattern.

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// get the last file name
// ./a/b/hello.txt -> hello.txt
char *
fmtname(char *path)
{
  char *p = path + strlen(path);
  while (p >= path && *p != '/') p--;
  return p + 1;
}

void
invoke(char **exec_args, int exec_argc, char *file)
{
  char *args[MAXARG];
  memmove(args, exec_args, exec_argc * sizeof(char*));
  args[exec_argc] = file;
  args[exec_argc + 1] = 0;
  if (fork() == 0) {
    exec(args[0], args);
    fprintf(2, "find: exec %s failed\n", args[0]);
    exit(1);
  }
  wait(0);
}

void
find(char *path, char *target, char **exec_args, int exec_argc)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0 || fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if (strcmp(fmtname(path), target) == 0)
      exec_args ? invoke(exec_args, exec_argc, path) : printf("%s\n", path);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      fprintf(2, "find: path too long\n");
      break;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if (de.inum == 0 || !strcmp(de.name, ".") || !strcmp(de.name, ".."))
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, target, exec_args, exec_argc); // recursive invoke the find method
    }
    break;
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc == 3) {
    find(argv[1], argv[2], 0, 0);
  } else if (argc >= 5 && strcmp(argv[3], "-exec") == 0) {
    find(argv[1], argv[2], &argv[4], argc - 4);
  } else {
    fprintf(2, "Usage: find <dir> <file> [-exec <cmd> [args...]]\n");
    exit(1);
  }
  exit(0);
}
```



## Overall Testing

After completing all exercises, run the following command to execute the full Lab 1 test suite and verify correctness:

```sh
./grade-lab-util
```

![2026-06-27 21.03.29](./2026-06-27%2021.03.29.png)
