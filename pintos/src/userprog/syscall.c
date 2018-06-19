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
#include "../devices/block.h"

static void syscall_handler(struct intr_frame *);
struct lock filesys_lock;

int practice(int i);
static void halt(void);
void exit(int status);
static pid_t exec(const char *file);
static int wait(pid_t pid);
bool create(const char *file_path, unsigned initial_size);
bool remove(const char *file_path);
int open(const char *file_path);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
bool chdir (const char *dir_path);
bool mkdir (const char *dir_path);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);

struct dir *get_start_dir (const char *file_path);
static int get_next_part (char part[NAME_MAX + 1], const char **src_p);
bool make_file_from_path (struct dir **parent, const char *file_path, bool is_dir, unsigned initial_size);
struct file *open_file_from_path (struct dir **parent, const char *file_path, char **file_name, block_sector_t *parent_sector);
bool get_dir_from_path (struct dir **parent, const char *dir_path);
block_sector_t get_cwd_inum (void);
struct dir *get_cwd (void);
bool set_cwd (struct dir *dir);
bool file_is_cwd (struct file *file);

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
bool create(const char *file_path, unsigned initial_size)
{
	lock_acquire(&filesys_lock);

	struct dir *start_dir = get_start_dir (file_path);

	bool result = make_file_from_path (&start_dir, file_path, false, initial_size);

	lock_release(&filesys_lock);
	return result;
}

/* 	remove file by name
	return success true or false */
bool remove(const char *file_path)
{
	bool result;
	lock_acquire(&filesys_lock);

	struct dir *dir = get_start_dir (file_path);

	char *file_name = NULL;
	block_sector_t parent_sector = 1;

	struct file *file = open_file_from_path (&dir, file_path, &file_name, &parent_sector);

	bool opened = file != NULL;
	bool remove = true;

	if (opened && file_is_dir (file))
	{
		if(file_is_open (file))
		{
			remove = false;
			file_close (file);
		}

		if (remove && file_is_cwd (file))
		{
			remove = false;
			file_close (file);
		}
	}

	if (remove)
	{
		file_close (file);
		result = filesys_remove (file_name, dir_open (inode_open (parent_sector)));
	}
		
	free(file_name);
	lock_release(&filesys_lock);
	return result;
}

/* 	open file
	return file descriptor or -1 if error occured */
int open(const char *file_path)
{
	lock_acquire(&filesys_lock);

	struct thread *cur = thread_current();
	int result = -1;

	struct dir *dir = get_start_dir (file_path);

	char *file_name = NULL;
	block_sector_t parent_sector = 1;

	struct file *file = open_file_from_path (&dir, file_path, &file_name, &parent_sector);

	if (file != NULL)
	{
		int i;
		for (i = 0; i < FD_MAX; i++)
		{
			if (cur->descls[i] == NULL)
			{
				cur->descls[i] = file;
				result = i + 2;
				break;
			}
		}
	}

	free (file_name);
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
		if (inode_is_dir (file_get_inode (cur->descls[fd - 2])))
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
	if (strlen (dir_path) == 0)
		return false;

	lock_acquire (&filesys_lock);
    
    struct dir *dir = get_start_dir (dir_path);

	bool result = get_dir_from_path (&dir, dir_path);

	result = result && set_cwd (dir);

	dir_close (dir);

    lock_release (&filesys_lock);

    return result;
}


bool mkdir(const char *dir_path)
{
	if (strlen (dir_path) == 0)
		return false;
	
	lock_acquire (&filesys_lock);

	struct dir *start_dir = get_start_dir (dir_path);

    bool result = make_file_from_path (&start_dir, dir_path, true, 128);

	lock_release (&filesys_lock);

	return result;
}

bool readdir(int fd, char *name)
{
	if (isdir (fd))
	{
		struct thread *cur = thread_current ();

		struct dir *dir = dir_open (file_get_inode (cur->descls[fd - 2]));

		bool result = dir_readdir (dir, name);

		if (result && (str_equal (name, ".", 2) || str_equal (name, "..", 3)))
			return readdir(fd, name);

		return result;
	}
	return false;
}

bool isdir(int fd)
{
	if (fd == 0 || fd == 1 || fd > FD_MAX + 1 || fd < 0)
	{
		return false;
	}
	struct thread *cur = thread_current();

	if (cur->descls[fd - 2] == NULL)
	{
		return false;
	}
	
	return inode_is_dir (file_get_inode (cur->descls[fd - 2]));
}

int inumber(int fd)
{
	if (fd == 0 || fd == 1 || fd > FD_MAX + 1 || fd < 0)
	{
		return -1;
	}

	struct thread *cur = thread_current ();

	if (cur->descls[fd - 2] == NULL)
	{
		return -1;
	}

	return inode_get_inumber (file_get_inode (cur->descls[fd - 2]));
}

struct dir *get_start_dir (const char *file_path)
{
	struct dir *start_dir = NULL;

    if (path_is_absolute (file_path))
    	start_dir = dir_open_root ();
    else
    	start_dir = get_cwd ();

    return start_dir;
}

bool make_file_from_path (struct dir **parent, const char *file_path, bool is_dir, unsigned initial_size)
{
	char *path_part = malloc (NAME_MAX + 1);
	char *last_part = malloc (NAME_MAX + 1);

	if (get_next_part (path_part, &file_path) != 1){
		free(path_part);
		free(last_part);
		return false;
	}

	strlcpy (last_part, path_part, NAME_MAX + 1);

	while (get_next_part (path_part, &file_path) == 1)
	{
		if (!(*parent = filesys_open_dir (*parent, last_part))){
			free(path_part);
			free(last_part);
			return false;
		}

		strlcpy (last_part, path_part, NAME_MAX + 1);
	}

	block_sector_t parent_sector = inode_get_inumber (dir_get_inode (*parent));

	if (!filesys_create (last_part, *parent, initial_size, is_dir)){
		free(path_part);
		free(last_part);
		return false;
	}

	if (is_dir)
	{
		struct inode *inode = NULL;
		*parent = dir_open (inode_open (parent_sector));
		dir_lookup (*parent, last_part, &inode);
		struct dir *dir = dir_open (inode);
		dir_add (dir, ".", inode_get_inumber (inode));
		dir_add (dir, "..", inode_get_inumber (dir_get_inode (*parent)));
		dir_close (dir);
		dir_close (*parent);
	}

	free(path_part);
	free(last_part);
	return true;
}

struct file *open_file_from_path (struct dir **parent, const char *file_path, char **file_name, block_sector_t *parent_sector)
{
	char *path_part = malloc (NAME_MAX + 1);
	char *last_part = malloc (NAME_MAX + 1);

	if (get_next_part (path_part, &file_path) != 1){
		strlcpy (last_part, path_part, NAME_MAX + 1);
		*file_name = last_part;
		free(path_part);

		if (str_equal (file_path, "/", 2))
			return file_open (dir_get_inode (*parent));
		else
			return NULL;
	}

	strlcpy (last_part, path_part, NAME_MAX + 1);

	while (get_next_part (path_part, &file_path) == 1)
	{
		if (!(*parent = filesys_open_dir (*parent, last_part))){
			*file_name = last_part;
			free(path_part);
			return NULL;
		}

		strlcpy (last_part, path_part, NAME_MAX + 1);
	}

	*file_name = last_part;
	free(path_part);
	*parent_sector = inode_get_inumber (dir_get_inode (*parent));
	return filesys_open (last_part, *parent);
}

bool get_dir_from_path (struct dir **parent, const char *dir_path)
{
	char *path_part = malloc (NAME_MAX + 1);

	if (get_next_part (path_part, &dir_path) != 1){
		if (str_equal (dir_path, "/", 2))
		{
			free(path_part);
			*parent = dir_open_root ();
			return true;
		}
		else
		{
			free(path_part);
			*parent = NULL;
			return false;
		}
	}

	do {
		if (!(*parent = filesys_open_dir (*parent, path_part))){
			free(path_part);
			return false;
		}
	} while (get_next_part (path_part, &dir_path) == 1);
	
	free(path_part);
	return true;
}

struct dir *get_cwd ()
{
	return dir_open (inode_open (get_cwd_inum ()));
}

block_sector_t get_cwd_inum ()
{
	return thread_current ()->curdir_inum;
}

bool set_cwd (struct dir *dir)
{
	if (dir == NULL)
		return false;

	thread_current ()->curdir_inum = inode_get_inumber (dir_get_inode (dir));
	return true;
}

bool file_is_cwd (struct file *file)
{
  return inode_get_inumber (file_get_inode (file)) == get_cwd_inum ();
}

static int get_next_part (char part[NAME_MAX + 1], const char **src_p) {
	const char *src = *src_p;
	char *dst = part;

	/* Skip leading slashes. If it’s all slashes, we’re done. */
	while (*src == '/')
		src++;

	if (*src == '\0')
		return 0;

	/* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
	while (*src != '/' && *src != '\0') {
		if (dst < part + NAME_MAX)
			*dst++ = *src;
		else
			return -1;
		src++;
	}

	*dst = '\0';

	/* Advance source pointer. */
	*src_p = src;
	return 1;
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
