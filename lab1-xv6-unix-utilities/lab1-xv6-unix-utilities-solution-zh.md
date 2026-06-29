# Lab 1 实验报告

[English](./lab1-xv6-unix-utilities-solution.md) · [中文](./lab1-xv6-unix-utilities-solution-zh.md)

**目标简述：** 实现若干用户态程序及 Unix 实用工具，熟悉 xv6 的开发环境与系统调用的基本使用方式。

**实验难度：** `sleep` (easy) · `sixfive` (moderate) · `memdump` (easy) · `find` (moderate) · `exec` (moderate)

## Boot xv6 (easy)

环境配置参考 lab tool setup 页面即可完成。macOS 环境下通过 Homebrew 安装相关工具链，安装过程中若遇到安全提示，在系统设置中信任对应程序后重新执行即可。



## sleep (easy)

本题要求在用户态实现 `sleep` 命令。

**参数处理**

参考 `user/echo.c` 可以了解 xv6 的命令行参数传递约定：`main` 函数的 `argc` 表示参数数量，`argv` 是参数字符串数组的指针。`echo` 正是通过这一机制将用户输入原样打印到屏幕的。

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

**实现思路**

1. 参数校验：参数数量不为 2 时输出错误信息并退出。
2. 系统调用：查阅 `user/user.h` 可找到 `int pause(int)` 系统调用，用于实现 sleep 功能。
3. 类型转换：命令行参数以字符串形式传入，需借助 `user/ulib.c` 中的 `atoi` 将其转换为整数。

新建 `user/sleep.c`，实现如下：

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

最后将 `_sleep` 添加至 `Makefile` 的 `UPROGS` 列表，重新编译即可运行。



## sixfive (moderate)

本题要求逐字符扫描输入文件，提取有效整数，仅打印能被 **5 或 6** 整除的数。遇到分隔符时数字结束；若在读取过程中遇到非分隔符、非数字字符（如 `xv6` 中的 `x`），则丢弃已累积的部分数字。

**参考 `cat` 的文件读取结构**

读取文件并逐字节处理，与 `cat` 命令的核心逻辑高度相似。查阅 `user/cat.c`：

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

`sixfive` 可复用 `cat` 的整体文件读取框架，区别仅在于对读取到的字节进行数字解析而非直接输出。

**实现思路**

引入两个状态变量：
- `in_num`：标记当前是否处于数字读取状态。
- `num`：累积当前读取到的数字值（通过 `num = num * 10 + (c - '0')` 逐位构建）。

分隔符的判断使用 `strchr` 函数。

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

同样将 `_sixfive` 添加至 `Makefile` 的 `UPROGS`，编译后执行测试即可验证正确性。



## memdump (easy)

本题无需新建文件，直接在 `user/memdump.c` 中实现 `void memdump(char *fmt, char *data)` 函数即可。

**实现思路**

`fmt` 为格式字符串，每个字符指定一次读取操作的类型；`data` 为待解析的原始字节流。通过将 `data` 转型为对应宽度的指针类型（如 `(int *)`、`(long *)` 等），可以让 CPU 以正确宽度一次性加载数据，避免逐字节读取再手动移位的繁琐操作。每次读取后将 `data` 指针向后移动对应的字节数。

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

**参考 `ls.c` 的目录遍历结构**

`user/ls.c` 展示了 xv6 中目录遍历的标准做法：通过 `open` 打开路径，`fstat` 获取文件元数据，再以 `switch` 按文件类型分支处理。

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

**实现思路**

`find` 需要遍历整棵文件树，采用递归深度优先搜索（DFS）实现。为避免无限递归，对 `.` 和 `..` 两个特殊目录项进行剪枝，不再向上或原地递归。

文件名匹配通过 `basename` 提取路径末段后与目标名称进行字符串比较（`strcmp`），不依赖完整路径。

`-exec` 功能单独抽象为 `invoke` 函数，通过 `fork` + `exec` + `wait` 的标准 Unix 进程控制模式执行子命令。

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



## Lab 1 整体测试

完成所有练习后，执行以下命令对 Lab 1 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-util
```

![2026-06-27 21.03.29](./2026-06-27%2021.03.29.png)
