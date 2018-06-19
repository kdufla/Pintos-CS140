#ifndef FILECACHE_FILECACHE_H
#define FILECACHE_FILECACHE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

#define CACHE_BLOCK_SIZE 512
/*
    Structure for cache;
*/
struct filecache {
    block_sector_t sector_idx;
    bool dirty;
    bool accessed;
    char* data;
};

void filecache_init(void);
void filecache_read_at(block_sector_t sector_idx, char* buffer, off_t bytes_read,  off_t size, off_t offset);
void filecache_write_at(block_sector_t sector_idx, char* buffer, off_t bytes_written, off_t size, off_t offset);
int is_in_filecache(block_sector_t sector_idx);
int get_cache_index(block_sector_t idx);
int  evicted(void);
void filecache_write_at_disk(block_sector_t sector_idx, char* buffer);
void filecache_read_from_disk (block_sector_t sector_idx, char* buffer);
void filecache_close(void);

#endif