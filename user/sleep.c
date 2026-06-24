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
