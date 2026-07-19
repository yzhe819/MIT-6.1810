#ifdef LAB_MMAP
typedef unsigned long size_t;
typedef long int off_t;
#endif

#define SBRK_ERROR ((char *)-1)

struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sys_sbrk(int,int);
int pause(int);
int uptime(void);
#ifdef LAB_NET
int bind(uint16);
int unbind(uint16);
int send(uint16, uint32, uint16, char *, uint32);
int recv(uint16, uint32*, uint16*, char *, uint32);
#endif
#ifdef LAB_PGTBL
int ugetpid(void);
uint64 pgpte(void*);
void kpgtbl(void);
#endif
#ifdef LAB_LOCK
int rwlktest(void);
int cpupin(int);
#endif
// mmap section
void* mmap(void *addr, int len, int prot, int flags, int fd, int offset);
int munmap(void *addr, int len);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
char* sbrk(int);
char* sbrklazy(int);
#ifdef LAB_LOCK
int statistics(void*, int);
#endif

// printf.c
void fprintf(int, const char*, ...) __attribute__ ((format (printf, 2, 3)));
void printf(const char*, ...) __attribute__ ((format (printf, 1, 2)));

// umalloc.c
void* malloc(uint);
void free(void*);
