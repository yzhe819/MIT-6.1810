#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// get the last file name
// ./a/b/hello.txt -> hello.txt
char*
fmtname(char *path)
{
  char *p;
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  return p;
}

void
find(char *path, char *target, char **exec_args, int exec_argc)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if(strcmp(fmtname(path), target) == 0){
      if(exec_args != 0){
        char *args[MAXARG];
        int i;
        for(i = 0; i < exec_argc; i++)
          args[i] = exec_args[i];
        args[exec_argc] = path;
        args[exec_argc + 1] = 0;

        int pid = fork();
        if(pid == 0){
          exec(args[0], args);
          fprintf(2, "find: exec %s failed\n", args[0]);
          exit(1);
        }
        wait(0);
      } else {
        printf("%s\n", path);
      }
    }
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
      if(de.inum == 0)
        continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", buf);
        continue;
      }
      find(buf, target, exec_args, exec_argc);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc == 3){
    find(argv[1], argv[2], 0, 0);
    exit(0);
  }
  if(argc >= 5 && strcmp(argv[3], "-exec") == 0){
    find(argv[1], argv[2], &argv[4], argc - 4);
    exit(0);
  }
  fprintf(2, "Usage: find <dir> <file> [-exec <cmd> [args...]]\n");
  exit(1);
}