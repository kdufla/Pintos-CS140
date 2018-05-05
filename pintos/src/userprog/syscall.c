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
#include "../lib/kernel/stdio.h"
#include "../devices/input.h"

static void syscall_handler(struct intr_frame *);
struct lock filesys_lock;

int practice(int i);
static void halt(void);
static void exit(int status);
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

void syscall_init(void)
{
	lock_init(&filesys_lock);
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool is_valid_address(void *p)
{
	if (p == NULL || !is_user_vaddr((uint32_t *)p) || pagedir_get_page(thread_current()->pagedir, (uint32_t *)p) == NULL)
	{
		return false;
	}

	return true;
}

/* syscalls */

int practice(int i)
{
	return i + 1;
}

static void halt(void)
{
	shutdown_power_off();
}

static void exit(int status)
{
	thread_current()->exit_status = status;
	thread_exit();
}

static pid_t exec(const char *file)
{
	return process_execute(file);
}

static int wait(pid_t pid)
{
	return process_wait(pid);
}

bool create(const char *file, unsigned initial_size)
{
	bool result;
	lock_acquire(&filesys_lock);
	result = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return result;
}

bool remove(const char *file)
{
	bool result;
	lock_acquire(&filesys_lock);
	result = filesys_remove(file);
	lock_release(&filesys_lock);
	return result;
}

int open(const char *file)
{
	lock_acquire(&filesys_lock);
	struct file_descriptor *fd;
	fd = palloc_get_page(PAL_ZERO);
	if (fd == NULL)
	{
		return -1;
	}
	struct thread *curr = thread_current();

	if (list_size(&(curr->file_descriptors)))
	{
		fd->id = list_entry(list_rbegin(&(curr->file_descriptors)), struct file_descriptor, descriptors)->id + 1;
	}
	else
	{
		fd->id = 2;
	}

	fd->file = filesys_open(file);
	list_push_back(&(curr->file_descriptors), &(fd->descriptors));
	lock_release(&filesys_lock);
	return fd->id;
}

int filesize(int fd)
{
	lock_acquire(&filesys_lock);
	struct list current_fd_list = thread_current()->file_descriptors;
	struct list_elem *e;
	int result = -1;

	for (e = list_begin(&current_fd_list); e != list_end(&current_fd_list); e = list_next(e))
	{
		struct file_descriptor *current_fd = list_entry(e, struct file_descriptor, descriptors);
		if (current_fd->id == fd)
		{
			result = file_length(current_fd->file);
			break;
		}
	}

	lock_release(&filesys_lock);
	return result;
}

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

	if (fd == 1)
	{
		return 0;
	}

	int result = 0;

	lock_acquire(&filesys_lock);
	struct list current_fd_list = thread_current()->file_descriptors;
	struct list_elem *e;

	for (e = list_begin(&current_fd_list); e != list_end(&current_fd_list); e = list_next(e))
	{
		struct file_descriptor *current_fd = list_entry(e, struct file_descriptor, descriptors);
		if (current_fd->id == fd)
		{
			result = file_read(current_fd->file, buffer, size);
			break;
		}
	}

	lock_release(&filesys_lock);
	return result;
}

int write(int fd, const void *buffer, unsigned size)
{
	if (fd == 0)
	{
		return 0;
	}

	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}

	int result = 0;

	lock_acquire(&filesys_lock);
	struct list current_fd_list = thread_current()->file_descriptors;
	struct list_elem *e;

	for (e = list_begin(&current_fd_list); e != list_end(&current_fd_list); e = list_next(e))
	{
		struct file_descriptor *current_fd = list_entry(e, struct file_descriptor, descriptors);
		if (current_fd->id == fd)
		{
			result = file_write(current_fd->file, buffer, size);
			break;
		}
	}

	lock_release(&filesys_lock);
	return result;
}

void seek(int fd, unsigned position)
{
	lock_acquire(&filesys_lock);
	struct list current_fd_list = thread_current()->file_descriptors;
	struct list_elem *e;
	e = list_begin(&current_fd_list);

	for (e = list_begin(&current_fd_list); e != list_end(&current_fd_list); e = list_next(e))
	{
		struct file_descriptor *current_fd = list_entry(e, struct file_descriptor, descriptors);
		if (current_fd->id == fd)
		{
			file_seek(current_fd->file, position);
			break;
		}
	}

	lock_release(&filesys_lock);
}

unsigned tell(int fd)
{
	lock_acquire(&filesys_lock);
	struct list current_fd_list = thread_current()->file_descriptors;
	struct list_elem *e;
	e = list_begin(&current_fd_list);
	int result = 0;

	for (e = list_begin(&current_fd_list); e != list_end(&current_fd_list); e = list_next(e))
	{
		struct file_descriptor *current_fd = list_entry(e, struct file_descriptor, descriptors);
		if (current_fd->id == fd)
		{
			result = file_tell(current_fd->file);
			break;
		}
	}

	lock_release(&filesys_lock);
	return result;
}

void close(int fd)
{
	lock_acquire(&filesys_lock);
	struct list current_fd_list = thread_current()->file_descriptors;
	struct list_elem *e;
	e = list_begin(&current_fd_list);

	for (e = list_begin(&current_fd_list); e != list_end(&current_fd_list); e = list_next(e))
	{
		struct file_descriptor *current_fd = list_entry(e, struct file_descriptor, descriptors);
		if (current_fd->id == fd)
		{
			list_remove(&(current_fd->descriptors));
			file_close(current_fd->file);
			break;
		}
	}

	lock_release(&filesys_lock);
}

/* given pointer and check if every byte of this pointer is valid */
static uint32_t *get_arg_int(uint32_t *p)
{
	if (is_valid_address(p) && is_valid_address((char *)p + 3))
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
static void *get_arg_pointer(char *p, int len)
{
	void *rv = (void *)p;

	if (len == NO_LEN)
	{
		do
		{
			if (!is_valid_address(p))
			{
				exit(-1);
			}
		} while (++p - 1);
	}
	else
	{
		while (len-- > 0)
		{
			if (!is_valid_address(p++))
			{
				exit(-1);
			}
		}
	}
	return rv;
}

#define GET_ARG_INT(i) (*get_arg_int(((uint32_t *)f->esp) + i))
#define GET_ARG_POINTER(i, len) (get_arg_pointer((char *)GET_ARG_INT(i), len))

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
		rv = (uint32_t)create(GET_ARG_POINTER(1, NO_LEN), GET_ARG_INT(2));
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
		practice(GET_ARG_INT(1));
		break;
	default:
		exit(-1);
	}

	// uint32_t* args = ((uint32_t*) f->esp);
	// printf("System call number: %d\n", args[0]);
	// if (args[0] == SYS_EXIT) {

	if (rv != 8675309)
	{
		f->eax = rv;
	}
	// 	printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
	// 	thread_exit();
	// }
}
