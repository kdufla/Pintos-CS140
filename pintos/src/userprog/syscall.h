#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "mmap.h"
#include "../threads/malloc.h"

typedef int pid_t;
typedef int mapid_t;

void syscall_init (void);
void exit(int status);
struct lock filesys_lock;


#endif /* userprog/syscall.h */
