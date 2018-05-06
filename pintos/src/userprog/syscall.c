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
	printf("%s: exit(%d)\n", thread_current()->name, status);
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

	struct thread *cur = thread_current();
	int result = -1;
	struct file *file_p = filesys_open(file);
	
	if(file_p != NULL){
		int i;
		for(i = 0; i < 64; i++){
			if(cur->descls[i] == NULL){
				cur->descls[i] = file_p;
				result = i + 2;
				break;	
			}
		}
	}

	lock_release(&filesys_lock);
	return result;
}

int filesize(int fd)
{
	lock_acquire(&filesys_lock);

	struct thread *cur = thread_current();	
	int result = -1;

	if(cur->descls[fd - 2] != NULL){
		result = file_length(cur->descls[fd - 2]);
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

	struct thread *cur = thread_current();	
	int result = 0;

	lock_acquire(&filesys_lock);

	if(cur->descls[fd - 2] != NULL){
		result = file_read(thread_current()->descls[fd - 2], buffer, size);
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

	struct thread *cur = thread_current();	
	int result = 0;

	lock_acquire(&filesys_lock);

	if(cur->descls[fd - 2] != NULL){
		result = file_write(thread_current()->descls[fd - 2], buffer, size);
	}

	lock_release(&filesys_lock);
	return result;
}

void seek(int fd, unsigned position)
{
	struct thread *cur = thread_current();	

	lock_acquire(&filesys_lock);

	if(cur->descls[fd - 2] != NULL){
		file_seek(thread_current()->descls[fd -2], position);
	}

	lock_release(&filesys_lock);
}

unsigned tell(int fd)
{
	struct thread *cur = thread_current();	
	int result = 0;

	lock_acquire(&filesys_lock);

	if(cur->descls[fd - 2] != NULL){
		result = file_tell(thread_current()->descls[fd -2]);
	}

	lock_release(&filesys_lock);
	return result;
}

void close(int fd)
{
	struct thread *cur = thread_current();	

	lock_acquire(&filesys_lock);

	if(cur->descls[fd - 2] != NULL){
		file_close(thread_current()->descls[fd - 2]);
		cur->descls[fd - 2] = NULL;
	}

	lock_release(&filesys_lock);
}

static bool is_valid_address(uint32_t *p)
{
	if (p == NULL || !is_user_vaddr((uint32_t *)p) || pagedir_get_page(thread_current()->pagedir, (uint32_t *)p) == NULL)
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
static void *get_arg_pointer(uint32_t *p, int len)
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
		} while (*(char *)++p - 1);
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
	// printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
	// 	thread_exit();
	// }
}
