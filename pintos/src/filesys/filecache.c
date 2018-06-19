#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/filecache.h"
#include "threads/synch.h"

#define CACHE_SIZE 64

struct filecache * cache;
int filled;
struct lock cache_lock;

void filecache_init()
{
    lock_init(&cache_lock);
    cache = malloc(sizeof(struct filecache) * CACHE_SIZE);
    filled = 0;
    int i;
    for(i=0; i< CACHE_SIZE; i++)
    {
        cache[i].sector_idx = block_size(fs_device);
        cache[i].data = malloc(BLOCK_SECTOR_SIZE);
    }
}

void filecache_read_at(block_sector_t sector_idx, char* buffer, off_t bytes_read, off_t size, off_t offset)
{
    lock_acquire(&cache_lock);
    struct filecache* current;
    int cache_index = is_in_filecache(sector_idx);
    if (cache_index != -1)
    {   
        current = &cache[cache_index];
    } else {
        if (filled == CACHE_SIZE) 
        {
            int evict = evicted();
            current = &cache[evict];    
        } else {
            current = &cache[filled];
            filled++;
        }        
        filecache_read_from_disk(sector_idx, current->data);
        current->dirty = 0;
    }
    current->accessed = 1;
    current->sector_idx = sector_idx;
    memcpy((char*)buffer + bytes_read, (char*) current->data + offset, size);
    lock_release(&cache_lock);
}

void filecache_write_at(block_sector_t sector_idx, char* buffer, off_t bytes_written, off_t size, off_t offset)
{
    lock_acquire(&cache_lock);
    struct filecache* current;
    int cache_index = is_in_filecache(sector_idx);
    if (cache_index != -1)
    {   
        current = &cache[cache_index];
    } else {
        if (filled == CACHE_SIZE) 
        {
            int evict = evicted();
            current = &cache[evict];    
        } else {
            current = &cache[filled];
            filled++;
        }        
        filecache_read_from_disk(sector_idx, current->data);
    }
    current->dirty = 1;
    current->accessed = 1;
    current->sector_idx = sector_idx;
    memcpy((char*) current->data + offset,(char*) buffer + bytes_written, size);
    lock_release(&cache_lock);
}

int is_in_filecache(block_sector_t sector_idx)
{
    int i;
    for( i=0; i<CACHE_SIZE; i++)
    {
        if(sector_idx == cache[i].sector_idx)
        {
            return i;
        }
    }
    return -1;
}

int evicted()
{
    int i;
    for(i=0; ; i++){
        struct filecache* current;
        current = &(cache[i % CACHE_SIZE]);
        if ( current->accessed == 0)
        {
            if( current->dirty == 1)
            {
                filecache_write_at_disk(current->sector_idx, current->data);
            }
            current->dirty = 0;
            return i % CACHE_SIZE;
        } else {
            current->accessed = 0;
        }
    }
    return 0;
}

void filecache_write_at_disk(block_sector_t sector_idx, char* buffer)
{
    block_write(fs_device, sector_idx, buffer);
}

void filecache_read_from_disk (block_sector_t sector_idx, char* buffer)
{
    block_read(fs_device, sector_idx, buffer);
}


void filecache_close()
{
    struct filecache* current;
    int i;
    for(i=0; i<CACHE_SIZE; i++){
        current = &cache[i];
        filecache_write_at_disk(current->sector_idx, current->data);
    }
}