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