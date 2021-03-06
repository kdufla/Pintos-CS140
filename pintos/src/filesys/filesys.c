#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/filecache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  filecache_init();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  filecache_close();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, struct dir *dir, off_t initial_size, bool is_dir)
{
  block_sector_t inode_sector = 0;
  if (dir == NULL)
    dir = dir_open_root();

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name, struct dir *dir)
{
  if (dir == NULL)
    dir = dir_open_root();
  
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name, struct dir *dir)
{
  if (dir == NULL)
    dir = dir_open_root();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");

  // Add . and .. to root directory
  struct dir *root = dir_open_root ();
  dir_add (root, ".", ROOT_DIR_SECTOR);
  dir_add (root, "..", ROOT_DIR_SECTOR);
  dir_close (root);

  free_map_close ();
  printf ("done.\n");
}

struct dir *
filesys_open_dir_recursively(struct dir * old, char *path)
{
  if (path == NULL)
    return NULL;

  dir_close (old);

  struct dir *dir = dir_open_root ();
  int index = 0;

  while (true)
  {
    char *b = str_find_char (path, index+1, '/');
    if (b == NULL)
      break;

    b[0] = '\0';

    filesys_open_dir (dir, path + index + 1);

    b[0] = '/';
    index = b - path;
  }

  return dir;
}

struct dir *
filesys_open_dir(struct dir *parent, const char *child_name)
{
  struct inode *inode = NULL;
  dir_lookup (parent, child_name, &inode);
  dir_close (parent);

  if (inode != NULL && inode_is_dir(inode))
    return dir_open (inode);

  return NULL;
}