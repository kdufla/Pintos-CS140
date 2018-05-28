#ifndef SWAP
#define SWAP

#include "page_table.h"
#include "frame_table.h"
#include "../devices/block.h"
#include "../lib/kernel/bitmap.h"
#include "../threads/synch.h"
#include "../threads/vaddr.h"


struct swap_map
{
	struct lock lock;
	struct bitmap *map;
};

void swap_init(void);
block_sector_t write_in_swap(void *addr);
void read_from_swap(block_sector_t bs, void *addr);

#endif