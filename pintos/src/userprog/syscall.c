#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"

static void syscall_handler (struct intr_frame *);


int practice (int i);
static void halt(void);
static void exit(int status);
static pid_t exec(const chat *file);
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
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* syscalls */


int practice (int i)
{
	return 1;
}

static void halt(void)
{
	shutdown_power_off(void);
}

static void exit(int status){

	struct thread *cur = thread_current ();

	lock_acquire (&(cur->free_lock));

  struct list_elem *e;
  struct list *list = &(cur->child_infos);
  struct child_info *info;
  for (e = list_next(list_begin (list)); e != list_tail (list); e = list_next (e))  // tril tu end?! sakitxavi ai es aris;  
  {
    info = list_entry(list_prev(e), struct child_info, elem);
    if (info->is_alive){
      info->is_alive = false;
    } else {
      list_remove(&(info->elem));
      palloc_free_page(info);
    }
  }

	lock_release (&(cur->free_lock));
  lock_acquire (cur->info->parent_free_lock);

  if (cur->info->is_alive){
    cur->info->is_alive = false;
    cur->info->status = status;
  } else {
    list_remove(&(cur->info->elem));
    palloc_free_page(cur->info);
  }

  lock_release (cur->info->parent_free_lock);

  lock_release(&(cur->info->exited_lock));
	thread_exit();
}

static pid_t exec(const chat *file){
	return process_execute(file);
}


static int wait(pid_t pid)
{
	return process_wait (pid);
}

#ifdef P3
bool create (const char *file, unsigned initial_size)
{

}

bool remove (const char *file)
{

}


int open (const char *file)
{

}


int filesize (int fd)
{

}


int read (int fd, void *buffer, unsigned size)
{

}


int write (int fd, const void *buffer, unsigned size)
{

}


void seek (int fd, unsigned position)
{

}


unsigned tell (int fd)
{

}


void close (int fd)
{

}

#endif

#define GET_ARG_INT(i) (*(((uint32_t*)f->esp) + i))
#define GET_ARG_POINTER(i) ((void*)GET_ARG_INT(i))

static void
syscall_handler (struct intr_frame *f UNUSED)
{
	int sysc = GET_ARG_INT(0);

	uint32_t rv = NULL;

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
		default:
			exit(-1);
	}



	// uint32_t* args = ((uint32_t*) f->esp);
	// printf("System call number: %d\n", args[0]);
	// if (args[0] == SYS_EXIT) {
	// 	f->eax = args[1];
	// 	printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
	// 	thread_exit();
	// }
}
