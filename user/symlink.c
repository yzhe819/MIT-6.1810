#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "Usage: symlink target path\n");
    exit(1);
  }

  if(symlink(argv[1], argv[2]) < 0){
    fprintf(2, "symlink: %s -> %s failed\n", argv[2], argv[1]);
    exit(1);
  }

  exit(0);
}