#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "../devices/shutdown.h"
#include "process.h"


static void syscall_handler (struct intr_frame *);
struct lock * filesys_lock;


int practice (int i);
static void halt(void);
static void exit(int status);
static pid_t exec(const char *file);
static int wait(pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void
syscall_init (void)
{
	lock_init(filesys_lock);
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* syscalls */


int practice (int i)
{
	return i+1;
}

static void halt(void)
{
	shutdown_power_off();
}

static void exit(int status){
	thread_current()->status = status;
	thread_exit();
}

static pid_t exec(const char *file){
	return process_execute(file);
}


static int wait(pid_t pid)
{
	return pid;
}
#define P3
#ifdef P3
bool create (const char *file, unsigned initial_size)
{
	bool result;
	lock_acquire(filesys_lock);
	result = filesys_create (file, initial_size);
	lock_release(filesys_lock);
	return result;
}

bool remove (const char *file)
{
	bool result;
	lock_acquire(filesys_lock);
	result = filesys_remove(file);
	lock_release(filesys_lock);
	return result;
}


int open (const char *file)
{
	return *(int*)file;
}


int filesize (int fd)
{
	return fd;
}


int read (int fd, void *buffer, unsigned size)
{
	return fd + *(int*)buffer + (int)size;
}


int write (int fd, const void *buffer, unsigned size)
{
	return fd + *(int*)buffer + (int)size;
}


void seek (int fd, unsigned position)
{
	int b = fd + position;
	b+=7;
}


unsigned tell (int fd)
{
	return (unsigned)fd;
}


void close (int fd)
{
	fd++;
}

#endif

#define GET_ARG_INT(i) (*(((uint32_t*)f->esp) + i))
#define GET_ARG_POINTER(i) ((void*)GET_ARG_INT(i))

static void
syscall_handler (struct intr_frame *f UNUSED)
{
	int sysc = GET_ARG_INT(0);

	uint32_t rv = 0;

	switch(sysc)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(GET_ARG_INT(1));
			break;
		case SYS_EXEC:
			rv = (uint32_t) exec(GET_ARG_POINTER(1));
			break;
		case SYS_WAIT:
			rv = (uint32_t) wait(GET_ARG_INT(1));
			break;
		case SYS_CREATE:
			rv = (uint32_t) create(GET_ARG_POINTER(1), GET_ARG_INT(2));
			break;
		case SYS_REMOVE:
			rv = (uint32_t) remove(GET_ARG_POINTER(1));
			break;
		case SYS_OPEN:
			rv = (uint32_t) open(GET_ARG_POINTER(1));
			break;
		case SYS_FILESIZE:
			rv = (uint32_t) filesize(GET_ARG_INT(1));
			break;
		case SYS_READ:
			rv = (uint32_t) read(GET_ARG_INT(1), GET_ARG_POINTER(2), GET_ARG_INT(3));
			break;
		case SYS_WRITE:
			rv = (uint32_t) write(GET_ARG_INT(1), GET_ARG_POINTER(2), GET_ARG_INT(3));
			break;
		case SYS_SEEK:
			seek(GET_ARG_INT(1), GET_ARG_INT(2));
			break;
		case SYS_TELL:
			rv = (uint32_t) tell(GET_ARG_INT(1));
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
		f->eax = rv;
	// 	printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
	// 	thread_exit();
	// }
}
