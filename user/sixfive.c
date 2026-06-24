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
