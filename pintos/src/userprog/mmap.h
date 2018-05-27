#ifndef MMAP_SYSCALL
#define MMAP_SYSCALL

#include "filesys/file.h"
#include "../lib/stddef.h"

struct mapel{
	size_t first;
	size_t last;
	struct file *file;
};






#endif