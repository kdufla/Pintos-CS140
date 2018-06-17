#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "../devices/shutdown.h"
#include "process.h"
#include "pagedir.h"
#include "../threads/vaddr.h"
#include "../filesys/filesys.h"
#include "../filesys/directory.h"
#include "../filesys/file.h"
#include "../filesys/inode.h"
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
bool chdir (const char *dir);
bool mkdir (const char *dir_path);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);

bool get_dir_and_filename(const char *file, char **b, struct dir **dir, char** full_path);
char *parse_path(const char *path, struct dir **dir);
static void copy_path(char *dest, const char *src);
static bool path_is_absolute(const char *path);

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

	struct dir *dir = NULL;
	char *b = NULL;
	char *fp = NULL;
	result = get_dir_and_filename(file, &b, &dir, &fp);
	free(fp);

	if (result)
		result = filesys_create(b, dir, initial_size, false);

	lock_release(&filesys_lock);
	return result;
}

/* 	remove file by name
	return success true or false */
bool remove(const char *file)
{
	bool result;
	lock_acquire(&filesys_lock);

	struct dir *dir = NULL;
	char *b = NULL;
	char *fp = NULL;
	result = get_dir_and_filename(file, &b, &dir, &fp);

	char* a = thread_current()->curdir;
	if (str_equal(a, fp, strlen(a)+1))
		result = false;

	free(fp);

	if (result)
		result = filesys_remove(b, dir);

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

	struct dir *dir = NULL;
	char *b = NULL;
	char *fp = NULL;

	

	bool success = get_dir_and_filename(file, &b, &dir, &fp);
	free(fp);


	if(success)
	{
		struct file *file_p;

		if(strlen(file) == 1 && file[0] == '/')
		{
			dir = dir_open_root();
			struct inode *inode = dir->inode;

			file_p = file_open(inode);
		}else{
			file_p = filesys_open(b, dir);
		}

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
		if (is_inode_dir(file_get_inode(cur->descls[fd - 2])))
			result = -1;
    	else
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

bool chdir(const char *dir_path)
{	
	struct dir * a = NULL;
    char *path = parse_path (dir_path, &a);
    copy_path (thread_current()->curdir, path);
    free (path);
    return true;
}


bool mkdir(const char *dir_path)
{
	if (strlen(dir_path) == 0)
		return false;
	
	lock_acquire(&filesys_lock);

	bool result = false;
	int len = strlen(dir_path);
	char *path = malloc(len + 1);
	copy_path(path, dir_path);

	if(path[len - 1] == '/')
		path[len - 1] = '\0';

	struct dir *dir = NULL;
	char *b = NULL;
	char *fp = NULL;
	result = get_dir_and_filename(path, &b, &dir, &fp);
	free(fp);


	if (result)
	{
		struct inode *inode = NULL;
		dir_lookup (dir, b, &inode);

		if (inode == NULL)
			result = filesys_create(b, dir, 128, true);
	}

	lock_release(&filesys_lock);
	free(path);
	return result;
}

bool readdir(int fd UNUSED, char *name UNUSED)
{
	return false;
}

bool isdir(int fd UNUSED)
{
	return false;
}

int inumber(int fd UNUSED)
{
	return -1;
}

char *parse_path(const char *dir_path, struct dir **dir)
{
	struct thread *th = thread_current ();
    
    char *path = malloc (strlen(th->curdir)+strlen(dir_path)+2);
    copy_path (path, th->curdir);

    if (path_is_absolute(dir_path))
    	copy_path (path, dir_path);
    else
    	copy_path (path + strlen(path), dir_path);

    if (path[strlen(path)-1] != '/')
    	copy_path (path + strlen(path), "/");

    long index = 0;

    struct dir *parent = dir_open_root ();

    if (parent == NULL)
		return NULL;

    while (path[index] != '\0')
    {
    	if ((int)strlen(path) > index + 3 && str_equal(path + index, "/../", 4))
    	{
    		if (index == 0)
    		{
				copy_path (path, path + 3);
    		}
    		else
    		{
    			char *b = str_find_char_reversed (path, index-1, '/');
    			copy_path (b, path + index + 3);
    			index = b - path;
    			b[0] = '\0';

    			parent = filesys_open_dir_recursively (parent, path);

    			if (parent == NULL)
					return NULL;

    			b[0] = '/';
    		}
		}
		else if ((int)strlen(path) > index + 2 && str_equal(path + index, "/./", 3))
		{
			copy_path (path + index, path + index + 2);
		}
		else if ((int)strlen(path) > index + 1 && str_equal(path + index, "//", 2))
		{
			copy_path (path + index, path + index + 1);
		}
		else
		{
			char *b = str_find_char (path, index+1, '/');
			if (b != NULL)
			{
				char *child = malloc (strlen(path)-index);
				copy_path (child, path+index+1);
				child[b-path-index-1] = '\0';

				parent = filesys_open_dir (parent, child);

				if (parent == NULL)
					return NULL;

				index = b - path;
				free (child);
			}
			else
			{
				break;
			}
		}
    }

    *dir = parent;
    return path;
}

bool get_dir_and_filename(const char *file, char **b, struct dir **dir, char** full_path)
{
	char *path;
	*b = strrchr(file, '/');
	if (*b == NULL)
	{
		path = malloc (1);
		path[0] = '\0';
		*b = file;
	}
	else
	{
		path = malloc (strlen(file) + 1);
		copy_path (path, file);
		path[*b-file] = '\0';
		(*b)++;
	}
	*full_path = parse_path (path, dir);
	bool result = *full_path != NULL;
	free(path);
	return result;
}

static void copy_path(char *dest, const char *src)
{
	int i = 0;
	int len = strlen(src);

	if (src > dest)
		for(;i < len + 1; i++)
			dest[i] = src[i];
	else
		for(i = len; i >= 0; i--)
			dest[i] = src[i];
}

static bool path_is_absolute(const char *path)
{
	return path[0] == '/';
}

/* check if pointer address is valid */
static bool is_valid_address(void *p)
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
static void *get_arg_pointer(void *p, int len)
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
		} while (*(char *)p++);
	}
	else
	{
		while (len-- > 0)
		{
			if (!is_valid_address((char *)p++))
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
	case SYS_CHDIR:
		rv = (uint32_t)chdir(GET_ARG_POINTER(1, NO_LEN));
		break;
	case SYS_MKDIR:
		rv = (uint32_t)mkdir(GET_ARG_POINTER(1, NO_LEN));
		break;
	case SYS_READDIR:
		rv = (uint32_t)readdir(GET_ARG_INT(1), GET_ARG_POINTER(2, NO_LEN));
		break;
	case SYS_ISDIR:
		rv = (uint32_t)isdir(GET_ARG_INT(1));
		break;
	case SYS_INUMBER:
		rv = (uint32_t)inumber(GET_ARG_INT(1));
		break;
	default:
		exit(-1);
	}

	if (rv != 8675309)
	{
		f->eax = rv;
	}
}
