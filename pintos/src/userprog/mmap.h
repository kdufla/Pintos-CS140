#ifndef MMAP_SYSCALL
#define MMAP_SYSCALL

#include "filesys/file.h"
#include "../lib/stddef.h"

struct mapel{
	void *first;
	void *last;
	struct file *file;
};






#endif