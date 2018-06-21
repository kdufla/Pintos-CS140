#include "swap.h"


void swap_init(void);
block_sector_t write_in_swap(void *addr);
void read_from_swap(block_sector_t bs, void *addr);

#define BLOCKS_IN_PAGE ((PGSIZE / BLOCK_SECTOR_SIZE) >= 1 ? (PGSIZE / BLOCK_SECTOR_SIZE) : 1)

struct swap_map smap;

void swap_init(void){
	lock_init(&smap.lock);
	struct block *swap = block_get_role(BLOCK_SWAP);
	smap.map = bitmap_create(block_size(swap) / BLOCKS_IN_PAGE);
}

block_sector_t write_in_swap(void *addr){
	lock_acquire(&smap.lock);
	
	struct block *swap = block_get_role(BLOCK_SWAP);
	int i = bitmap_scan_and_flip(smap.map, 0, 1, 0); // free page in swap
	block_sector_t ir = i * BLOCKS_IN_PAGE; // real addres in swap

	for(i = 0; i < BLOCKS_IN_PAGE; i++, addr += PGSIZE / BLOCKS_IN_PAGE){
		block_write(swap, ir + i, addr);
	}

	lock_release(&smap.lock);

	return ir / BLOCKS_IN_PAGE;
}

void read_from_swap(block_sector_t bs, void *addr){
	
	lock_acquire(&smap.lock);
	
	struct block *swap = block_get_role(BLOCK_SWAP);
	bitmap_set(smap.map, bs, 0);
	block_sector_t i, bsr = bs * BLOCKS_IN_PAGE;

	void *kaddr = pagedir_get_page(thread_current()->pagedir, addr);
	for(i = 0; i < BLOCKS_IN_PAGE; i++, kaddr += PGSIZE / BLOCKS_IN_PAGE){
		block_read(swap, bsr + i, kaddr);
	}

	lock_release(&smap.lock);
}
