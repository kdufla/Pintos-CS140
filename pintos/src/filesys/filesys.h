#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "directory.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, struct dir *dir, off_t initial_size, bool is_dir);
struct file *filesys_open (const char *name, struct dir *dir);
bool filesys_remove (const char *name, struct dir *dir);
struct dir *filesys_open_dir_recursively(struct dir * old, char *path);
struct dir *filesys_open_dir(struct dir *parent, const char *child_name);

#endif /* filesys/filesys.h */
