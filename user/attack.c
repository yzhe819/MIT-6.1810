#include "kernel/types.h"
#include "user/user.h"

#define DATASIZE (8 * 4096)

int 
main(void) {
  char *mem = sbrk(DATASIZE);
  const char *hint = "This may help.";

  for (int i = 0; i < DATASIZE - 16; i++) {
    if (!strcmp(mem + i, hint)) {
      printf("%s\n", mem + i + 16);
      break;
    }
  }

  exit(0);
}