#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_BLOCKS 123
#define ADDS_IN_BLOCK (BLOCK_SECTOR_SIZE / sizeof(block_sector_t))

#define number_of_directs(sec) (sec > DIRECT_BLOCKS ? DIRECT_BLOCKS : sec)
#define realsinds(sec) (sec > ADDS_IN_BLOCK ? ADDS_IN_BLOCK : sec)
#define number_of_block_with_arrays(sec) (number_of_directs(sec) == DIRECT_BLOCKS ? realsinds(sec - DIRECT_BLOCKS) : 0)
#define realdinds(sec) (sec - ADDS_IN_BLOCK - DIRECT_BLOCKS)
#define number_of_dindirects(sec) (number_of_block_with_arrays(sec) == ADDS_IN_BLOCK ? realdinds(sec) : 0)
#define number_of_dindirect_blocks(sec) (number_of_dindirects(sec) / ADDS_IN_BLOCK + number_of_dindirects(sec) % ADDS_IN_BLOCK > 0 ? 1 : 0)
#define number_till_end(sec, i) ((number_of_dindirects(sec) - i * ADDS_IN_BLOCK) >= ADDS_IN_BLOCK ? ADDS_IN_BLOCK : (number_of_dindirects(sec) - i * ADDS_IN_BLOCK))
#define DOUBLY_IDX(sec) (realdinds(sec) / ADDS_IN_BLOCK)
#define DOUBLY_IDX_IN(sec) (realdinds(sec) % ADDS_IN_BLOCK)
#define MIN(a, b) (a > b ? b : a)
#define blocks_needed(sectors) (sectors + (sectors > DIRECT_BLOCKS ? 1 : 0) + (sectors > DIRECT_BLOCKS + BLOCK_SECTOR_SIZE / 4 ? 1 + DIV_ROUND_UP((sectors - DIRECT_BLOCKS - BLOCK_SECTOR_SIZE / 4), (BLOCK_SECTOR_SIZE / 4)) : 0))



/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                         /* File size in bytes. */
    block_sector_t direct[DIRECT_BLOCKS]; /* Direct Blocks */
    block_sector_t single;                /* Single Indirect */
    block_sector_t doubly;                /* Double Indirect */
    bool is_dir;
    unsigned magic;                       /* Magic number. */
  };

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    off_t pos;
    struct inode_disk data;             /* Inode content. */
  };
struct block_with_array {
  block_sector_t sectors[ADDS_IN_BLOCK];  /* 128 numbers, 4 byte each (512 bytes or 1 block total) */
};


block_sector_t *get_memory_on_disk(size_t sectors);
void fill_direct_sectors(int sectors,char *zeros, struct inode_disk *disk_inode, block_sector_t *addrs);
void fill_indirect_sectors(int sectors, char *zeros, int curr, struct block_with_array *sd, block_sector_t *addrs);
void release_inode_mem(struct inode_disk *id);

void fill_gap(struct inode_disk *id, size_t off);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}



/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
  {
    size_t sec = pos /  BLOCK_SECTOR_SIZE;
    struct inode_disk *id = &inode->data;

    if(sec < DIRECT_BLOCKS)
    {
      return id->direct[sec];
    }

    if(sec < DIRECT_BLOCKS + ADDS_IN_BLOCK)
    {
      struct block_with_array *sd = malloc(sizeof(struct block_with_array));
      block_read (fs_device, id->single, sd);

      block_sector_t bs = sd->sectors[sec - DIRECT_BLOCKS];

      free(sd);
      return bs;
    }

    struct block_with_array *sd = malloc(sizeof(struct block_with_array));
    block_read (fs_device, id->doubly, sd);

    block_sector_t bs = sd->sectors[DOUBLY_IDX(sec)];
    block_read (fs_device, bs, sd);

    return sd->sectors[realdinds(sec) % ADDS_IN_BLOCK];
  }
  else
    return -1;
}

/* Returns the block device sector that contains byte offset POS
 * within INODE.
 * Creates and returns the block device sector that contains byte 
 * offset POS within INODE if INODE does not contain data for a byte
 * at offset POS.
 * */
static block_sector_t
byte_to_sector_create (struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
  {
    return byte_to_sector(inode, pos);
  }
  else
  {
    size_t sec = pos /  BLOCK_SECTOR_SIZE;
    struct inode_disk *id = &inode->data;
    block_sector_t baddr;

    if(sec < DIRECT_BLOCKS)
    {
      if(id->direct[sec] > 0){
        return id->direct[sec];
      }
      free_map_allocate (1, &baddr);
      id->direct[sec] = baddr;
      block_write(fs_device, inode->sector, id);
      return baddr;
    }

    if(sec < DIRECT_BLOCKS + ADDS_IN_BLOCK)
    {
      struct block_with_array *sd = calloc(1, sizeof(struct block_with_array));
      if(id->single > 0)
      {
        block_read (fs_device, id->single, sd);
      }else
      {
        free_map_allocate (1, &baddr);
        id->single = baddr;
      }

      if(sd->sectors[sec - DIRECT_BLOCKS] > 3)
      {
        baddr = sd->sectors[sec - DIRECT_BLOCKS];
        free(sd);
        return baddr;
      }

      free_map_allocate (1, &baddr);
      sd->sectors[sec - DIRECT_BLOCKS] = baddr;

      block_write(fs_device, id->single, sd);

      free(sd);
      return baddr;
    }

    struct block_with_array *sd = calloc(1, sizeof(struct block_with_array));
    struct block_with_array *ssd = calloc(1, sizeof(struct block_with_array));
    if(id->doubly > 0){
      block_read (fs_device, id->doubly, sd);
    }else
    {
      free_map_allocate (1, &baddr);
      id->doubly = baddr;      
    }

    block_sector_t bs = sd->sectors[DOUBLY_IDX(sec)];
    if(bs > 0)
    {
      block_read (fs_device, bs, ssd);
    }else
    {
      free_map_allocate (1, &baddr);
      sd->sectors[DOUBLY_IDX(sec)] = baddr;
      block_write (fs_device, id->doubly, sd);
    }

    if(ssd->sectors[DOUBLY_IDX_IN(sec)] > 0)
    {
      baddr = ssd->sectors[DOUBLY_IDX_IN(sec)];
    }else
    {
      free_map_allocate (1, &baddr);
      ssd->sectors[DOUBLY_IDX_IN(sec)] = baddr;
      block_write (fs_device, sd->sectors[DOUBLY_IDX(sec)], ssd);
    }
    
    free(sd);
    free(ssd);
    return baddr;
  }
}


/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/*
 * Get array of len non-consecutive sectors 
*/
block_sector_t *get_memory_on_disk(size_t len)
{
  size_t i, j;

  block_sector_t *addrs = malloc(sizeof(block_sector_t) * len), baddr;
  
  for(i = 0; i < len; i++)
  {
    if(free_map_allocate (1, &baddr))
    {
      addrs[i] = baddr;
    }else{
      for(j = 0; j < i; j++)
      {
        free_map_release(addrs[j], 1);
      }
      free(addrs);
      return NULL;
    }
  }

  return addrs;
}

/*
 * fill directly indexed sectors (first int sectors)
 */
void fill_direct_sectors(int sectors,char *zeros, struct inode_disk *disk_inode, block_sector_t *addrs)
{
  int i;
  for(i = 0; i < sectors; i++)
  {
      block_write (fs_device, addrs[i], zeros);
      disk_inode->direct[i] = addrs[i];
  }
}

/*
 * fill struct block_with_array *sd with sectors (first int sectors)
 */
void fill_indirect_sectors(int sectors, char *zeros, int curr, struct block_with_array *sd, block_sector_t *addrs)
{
  int i;
  for(i = 0; i < sectors; i++)
  {
      block_write (fs_device, addrs[i + curr], zeros);
      sd->sectors[i] = addrs[i + curr];
  }
}

/*
 * allocate memory on dist if needed.
 * from last allocated memory (lenght) to new offset
 * and fill with 0's
 */
void fill_gap(struct inode_disk *id, size_t off)
{
  size_t lsec = id->length /  BLOCK_SECTOR_SIZE, osec = off /  BLOCK_SECTOR_SIZE, i, j, count = 0, lll = id->length;
  
  if(lsec >= osec){
    return;
  }
  static char zeros[BLOCK_SECTOR_SIZE];
  memset(zeros, 0, BLOCK_SECTOR_SIZE);
  block_sector_t *addrs = get_memory_on_disk(blocks_needed(bytes_to_sectors(off)) - blocks_needed(bytes_to_sectors(lll)));
  
  while(lsec < DIRECT_BLOCKS && lsec < osec)
  {
    id->direct[lsec++] = addrs[count];
    block_write (fs_device, addrs[count++], zeros);
  }

  if(lsec < osec)
  {
    struct block_with_array *sd = calloc(1, sizeof(struct block_with_array));

    if(lsec < DIRECT_BLOCKS + ADDS_IN_BLOCK)
    {
      bool new_single = lsec == DIRECT_BLOCKS;
      if(!new_single)
      {
        block_read (fs_device, id->single, sd);
      }

      while(lsec < osec && lsec < DIRECT_BLOCKS + ADDS_IN_BLOCK)
      {
        sd->sectors[lsec++ - DIRECT_BLOCKS] = addrs[count];  
        block_write (fs_device, addrs[count++], zeros);
      }

      if(new_single)
      {
        id->single = addrs[count];
        block_write (fs_device, addrs[count++], sd);
      }else{
        block_write (fs_device, id->single, sd);
      }
    }

    if(lsec < osec){
      struct block_with_array *ssd = calloc(1, sizeof(struct block_with_array));
      memset(sd, 0, sizeof(struct block_with_array));

      bool new_double = lsec == DIRECT_BLOCKS + ADDS_IN_BLOCK;
      if(!new_double)
      {
        block_read (fs_device, id->doubly, sd);
      }
      
      
      for(i = number_of_dindirect_blocks(lsec); i <= number_of_dindirect_blocks(osec); i++)
      {
        bool new_dentry = realdinds(lsec) % ADDS_IN_BLOCK == 0;
        if(!new_dentry)
        {
          block_read (fs_device, sd->sectors[i], ssd);
        }

        size_t n = number_till_end(osec, i) - number_till_end(lsec, i);
        for(j = 0; j < n; j++)
        {
          ssd->sectors[(lsec++ - DIRECT_BLOCKS - ADDS_IN_BLOCK) % ADDS_IN_BLOCK] = addrs[count];  
          block_write (fs_device, addrs[count++], zeros);
        }

        if(new_dentry)
        {
          sd->sectors[i] = addrs[count];
          block_write (fs_device, addrs[count++], ssd);
        }else{
          block_write (fs_device, sd->sectors[i], ssd);
        }
        
      }
      
      if(new_double)
      {
        id->doubly = addrs[count];
        block_write (fs_device, addrs[count++], sd);
      }else{
        block_write (fs_device, id->doubly, sd);
      }
    }
  }
}


/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length), curr_count = 0, count = 0, i;
      static char zeros[BLOCK_SECTOR_SIZE];
      memset(zeros, 0, BLOCK_SECTOR_SIZE);
      disk_inode->length = length;
      disk_inode->is_dir = is_dir;
      disk_inode->magic = INODE_MAGIC;

      if(length == 0){
        block_write (fs_device, sector, disk_inode);
        free(disk_inode);
        return true;
      }

      block_sector_t *addrs = get_memory_on_disk(blocks_needed(sectors));

      if(!addrs)
      {
        free(disk_inode);
        return false;
      }


      fill_direct_sectors(number_of_directs(sectors), zeros, disk_inode, addrs);
      curr_count += number_of_directs(sectors);
      count += number_of_directs(sectors);

      if(count >= sectors)
      {
        block_write (fs_device, sector, disk_inode);
        free(addrs);
        free(disk_inode);
        return true;
      }

      struct block_with_array *sd = malloc(512);

      fill_indirect_sectors(number_of_block_with_arrays(sectors), zeros, curr_count, sd, addrs);
      curr_count += number_of_block_with_arrays(sectors);
      count += number_of_block_with_arrays(sectors);

      block_write (fs_device, addrs[curr_count], sd);
      disk_inode->single = addrs[curr_count++];

      if(count >= sectors)
      {
        block_write (fs_device, sector, disk_inode);
        free(addrs);
        free(disk_inode);
        free(sd);
        return true;
      }

      struct block_with_array *ssd = malloc(512);
      memset(sd, 0, 512);
      
      for(i = 0; i < number_of_dindirect_blocks(sectors); i++)
      {
        size_t b = number_till_end(sectors, i);
        memset(ssd, 0, 512);

        fill_indirect_sectors(b, zeros, curr_count, ssd, addrs);
        curr_count += b;
        count += b;
        
        block_write (fs_device, addrs[curr_count], ssd);
        sd->sectors[i] = addrs[curr_count++];
      }

      block_write (fs_device, addrs[curr_count], sd);
      disk_inode->doubly = addrs[curr_count++];
      
      block_write (fs_device, sector, disk_inode);

      free(addrs);
      free(disk_inode);
      free(sd);
      free(ssd);
      return true;
    }

    return false;
}


/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->pos = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/*
 * release memory on disk held by passed inode
 */
void release_inode_mem(struct inode_disk *id)
{
  size_t i, j, n, b, sec = bytes_to_sectors(id->length);

  n = MIN(sec, DIRECT_BLOCKS);
  for(i = 0; i < n; i++)
  {
    free_map_release(id->direct[i], 1);
  }

  if(sec > DIRECT_BLOCKS)
  {
    struct block_with_array *sd = malloc(sizeof(struct block_with_array));
    block_read (fs_device, id->single, sd);

    n = MIN(sec - n, ADDS_IN_BLOCK);
    for( i = 0; i < n; i++)
    {
      free_map_release(sd->sectors[i], 1);
    }
    
    if(sec > DIRECT_BLOCKS + ADDS_IN_BLOCK)
    {
      block_read (fs_device, id->doubly, sd);
      struct block_with_array *ssd = malloc(sizeof(struct block_with_array));
      
      n = number_of_dindirect_blocks(sec);
      for(i = 0; i < n; i++)
      {
        block_read (fs_device, sd->sectors[i], ssd);

        b = number_till_end(sec, i);
        for( j = 0; j < b; j++)
        {
          free_map_release(ssd->sectors[j], 1);
        }
      }

      free(ssd);
    }

    free(sd);
  }
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          free_map_release (inode->sector, 1);
          release_inode_mem(&inode->data);
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
  {
    return 0;
  }

  struct inode_disk *id = &inode->data;
  fill_gap(id, offset);

  while (size > 0)
    {

      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector_create (inode, offset);


      sector_idx = byte_to_sector_create (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      // off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = sector_left; // inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
   
      if(id->length < offset + size){
        id->length = offset + chunk_size;
      }
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  block_write (fs_device, inode->sector, id);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_is_dir (struct inode *inode)
{
  return inode->data.is_dir;
}

int
inode_open_count (struct inode *inode)
{
  return inode->open_cnt;
}

off_t
inode_pos (struct inode *inode)
{
  return inode->pos;
}

void
inode_incremente_pos (struct inode *inode, int n)
{
  inode->pos += n;
}
