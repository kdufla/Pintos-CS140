#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "../devices/shutdown.h"
#include "process.h"
#include "pagedir.h"
#include "../threads/vaddr.h"
#include "../filesys/filesys.h"
#include "../filesys/file.h"
#include "../lib/kernel/stdio.h"
#include "../devices/input.h"

static void syscall_handler(struct intr_frame *);
struct lock filesys_lock;

int practice(int i);
static void halt(void);
void exit(int status);
static pid_t exec(const char *file);
static int wait(pid_t pid);

bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

mapid_t mmap(int fd, void *addr);
void munmap(mapid_t);

void syscall_init(void)
{
	lock_init(&filesys_lock);
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* syscalls */

/* practice syscall - ++i */
int practice(int i)
{
	return i + 1;
}

/* Terminate Pintos */
static void halt(void)
{
	shutdown_power_off();
}

/* Terminate user program and passes exit status to kernel */
void exit(int status)
{
	thread_current()->exit_status = status;
	printf("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}

/* execute executable file by name */
static pid_t exec(const char *file)
{
	return process_execute(file);
}

static int wait(pid_t pid)
{
	return process_wait(pid);
}

/* 	create file with initial size - initial_size and name - file
	return success true or false */
bool create(const char *file, unsigned initial_size)
{
	bool result;
	lock_acquire(&filesys_lock);
	result = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return result;
}

/* 	remove file by name
	return success true or false */
bool remove(const char *file)
{
	bool result;
	lock_acquire(&filesys_lock);
	result = filesys_remove(file);
	lock_release(&filesys_lock);
	return result;
}

/* 	open file
	return file descriptor or -1 if error occured */
int open(const char *file)
{
	lock_acquire(&filesys_lock);

	struct thread *cur = thread_current();
	int result = -1;
	struct file *file_p = filesys_open(file);

	if (file_p != NULL)
	{
		int i;
		for (i = 0; i < FD_MAX; i++)
		{
			if (cur->descls[i] == NULL)
			{
				cur->descls[i] = file_p;
				result = i + 2;
				break;
			}
		}
	}

	lock_release(&filesys_lock);
	return result;
}

/*	get size of file
	return -1 if errorr occured */
int filesize(int fd)
{
	int result = -1;

	if (fd == 0 || fd == 1 || fd > FD_MAX + 1 || fd < 0)
	{
		return result;
	}
	lock_acquire(&filesys_lock);

	struct thread *cur = thread_current();

	if (cur->descls[fd - 2] != NULL)
	{
		result = file_length(cur->descls[fd - 2]);
	}

	lock_release(&filesys_lock);
	return result;
}

/*	read file and copy "size" bytes into buffuer
	return number of bytes actually copied */
int read(int fd, void *buffer, unsigned size)
{
	if (fd == 0)
	{
		while (size--)
		{
			*(char *)buffer++ = input_getc();
		}
		return size;
	}

	if (fd == 1 || fd > FD_MAX + 1 || fd < 0)
	{
		return 0;
	}

	struct thread *cur = thread_current();
	int result = 0;

	lock_acquire(&filesys_lock);

	if (cur->descls[fd - 2] != NULL)
	{
		result = file_read(thread_current()->descls[fd - 2], buffer, size);
	}

	lock_release(&filesys_lock);
	return result;
}

/*	read buffer and copy "size" bytes into file
	return number of bytes actually copied */
int write(int fd, const void *buffer, unsigned size)
{
	if (fd == 0 || fd > FD_MAX + 1 || fd < 0)
	{
		return 0;
	}

	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}

	struct thread *cur = thread_current();
	int result = 0;

	lock_acquire(&filesys_lock);

	if (cur->descls[fd - 2] != NULL)
	{
		result = file_write(cur->descls[fd - 2], buffer, size);
	}

	lock_release(&filesys_lock);
	return result;
}

/*	Sets the current position in file to position bytes from the
	start of the file. */
void seek(int fd, unsigned position)
{
	if (fd == 0 || fd == 1 || fd > FD_MAX + 1 || fd < 0)
	{
		return;
	}
	struct thread *cur = thread_current();

	lock_acquire(&filesys_lock);

	if (cur->descls[fd - 2] != NULL)
	{
		file_seek(thread_current()->descls[fd - 2], position);
	}

	lock_release(&filesys_lock);
}

/* return current position in file */
unsigned tell(int fd)
{
	if (fd == 0 || fd == 1 || fd > FD_MAX + 1 || fd < 0)
	{
		return 0;
	}
	struct thread *cur = thread_current();
	int result = 0;

	lock_acquire(&filesys_lock);

	if (cur->descls[fd - 2] != NULL)
	{
		result = file_tell(thread_current()->descls[fd - 2]);
	}

	lock_release(&filesys_lock);
	return result;
}

/* close file */
void close(int fd)
{
	if (fd == 0 || fd == 1 || fd > FD_MAX + 1 || fd < 0)
	{
		return;
	}
	struct thread *cur = thread_current();

	lock_acquire(&filesys_lock);

	if (cur->descls[fd - 2] != NULL)
	{
		file_close(thread_current()->descls[fd - 2]);
		cur->descls[fd - 2] = NULL;
	}

	lock_release(&filesys_lock);
}

#define OVERLAP(a, b, x, y) ((x >= a && x <= b) || (y >= a && y <= b))

static bool is_overlap(struct mapel *mp, size_t first, size_t last)
{
	size_t i;
	for (i = 0; i < MAP_MAX; i++)
	{
		if (mp[i].first != 0 && OVERLAP(mp[i].first, mp[i].last, first, last))
		{
			return true;
		}
	}
	return false;
}

mapid_t mmap(int fd, void *addr)
{
	if (fd == 0 || fd == 1 || fd > FD_MAX + 1 || fd < 0 || (size_t)addr % PGSIZE != 0)
	{
		return -1;
	}

	struct thread *th = thread_current();

	struct file *file = th->descls[fd - 2];
	if (file == NULL)
	{
		return -1;
	}
	file = file_reopen(file);

	size_t i, first = (size_t)addr, last, size_bytes, size_pages, zeros;

	size_bytes = file_length(file);
	size_pages = size_bytes / PGSIZE + size_bytes % PGSIZE == 0 ? 0 : 1;
	zeros = size_pages * PGSIZE - size_bytes;

	last = first + size_pages;

	if (is_overlap(th->maps, first, last))
	{
		return -1;
	}

	for (i = 0; i < MAP_MAX; i++)
	{
		if (th->maps[i].first == 0)
		{
			th->maps[i].first = first;
			th->maps[i].last = last;
			th->maps[i].file = file;

			size_t n;

			for (n = 0; n < size_pages; n++)
			{
				struct supl_page *sp = malloc(sizeof(struct supl_page));
				memset(sp, 0, sizeof(struct supl_page));

				sp->addr = addr;
				sp->mapid = i;
				if (n == size_pages - 1)
				{
					sp->zero_bytes = zeros;
				}

				hash_insert(&th->pages, &sp->hash_elem);

				addr += PGSIZE;
			}

			return i;
		}
	}

	return -1;
}

void munmap(mapid_t);

/* check if pointer address is valid */
static bool is_valid_address(void *p)
{
	if (p == NULL || !is_user_vaddr((uint32_t *)p) || pagedir_get_page(thread_current()->pagedir, (uint32_t *)p) == NULL)
	{
		return false;
	}

	return true;
}

/* check if pointer address is valid */
static bool is_valid_address_p(void *p)
{
	if (p == NULL || !is_user_vaddr((uint32_t *)p))
	{
		return false;
	}

	return true;
}

/* given pointer and check if every byte of this pointer is valid */
static uint32_t *get_arg_int(uint32_t *p)
{
	if (is_valid_address(p) && is_valid_address((uint32_t *)((char *)p + 3)))
	{
		return p;
	}

	exit(-1);

	return p;
}

#define NO_LEN -7

/* given pointer p of memory with size len bytes
 * if len is NO_LEN it's guaranteed that memory ends with NULL/'\0' (false)
 * check validity of every byte's address
 * */
static void *get_arg_pointer(void *p, int len)
{
	void *rv = (void *)p;

	if (len == NO_LEN)
	{
		do
		{
			if (!is_valid_address_p(p))
			{
				exit(-1);
			}
		} while (*(char *)p++);
	}
	else
	{
		while (len-- > 0)
		{
			if (!is_valid_address_p((char *)p++))
			{
				exit(-1);
			}
		}
	}
	return rv;
}

#define GET_ARG_INT(i) (*get_arg_int(((uint32_t *)f->esp) + i))

#define GET_ARG_POINTER(i, len) (get_arg_pointer((uint32_t *)GET_ARG_INT(i), len))

static void
syscall_handler(struct intr_frame *f UNUSED)
{
	int sysc_num = GET_ARG_INT(0);

	uint32_t rv = 8675309;

	switch (sysc_num)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(GET_ARG_INT(1));
		break;
	case SYS_EXEC:
		rv = (uint32_t)exec(GET_ARG_POINTER(1, NO_LEN));
		break;
	case SYS_WAIT:
		rv = (uint32_t)wait(GET_ARG_INT(1));
		break;
	case SYS_CREATE:
		rv = (uint32_t)create(GET_ARG_POINTER(1, NO_LEN), (unsigned)GET_ARG_INT(2));
		break;
	case SYS_REMOVE:
		rv = (uint32_t)remove(GET_ARG_POINTER(1, NO_LEN));
		break;
	case SYS_OPEN:
		rv = (uint32_t)open(GET_ARG_POINTER(1, NO_LEN));
		break;
	case SYS_FILESIZE:
		rv = (uint32_t)filesize(GET_ARG_INT(1));
		break;
	case SYS_READ:
		rv = (uint32_t)read(GET_ARG_INT(1), GET_ARG_POINTER(2, GET_ARG_INT(3)), GET_ARG_INT(3));
		break;
	case SYS_WRITE:
		rv = (uint32_t)write(GET_ARG_INT(1), GET_ARG_POINTER(2, GET_ARG_INT(3)), GET_ARG_INT(3));
		break;
	case SYS_SEEK:
		seek(GET_ARG_INT(1), GET_ARG_INT(2));
		break;
	case SYS_TELL:
		rv = (uint32_t)tell(GET_ARG_INT(1));
		break;
	case SYS_CLOSE:
		close(GET_ARG_INT(1));
		break;
	case SYS_PRACTICE:
		rv = practice(GET_ARG_INT(1));
		break;
	default:
		exit(-1);
	}

	if (rv != 8675309)
	{
		f->eax = rv;
	}
}
