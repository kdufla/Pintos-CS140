#include "vm/frame_table.h"

struct f_table frame_table;

void frame_table_init()
{
	list_init(&frame_table.frame_ls);
	lock_init(&frame_table.frame_lock);
	uint8_t *free_start = ptov(1024 * 1024);
	uint8_t *free_end = ptov(init_ram_pages * PGSIZE);
	size_t free_pages = (free_end - free_start) / PGSIZE;
	size_t user_pages = free_pages / 2;
	frame_table.frame_ls_array = malloc(sizeof(struct frame) * user_pages);

	ASSERT(frame_table.frame_ls_array != NULL)

	memset(frame_table.frame_ls_array, 0, sizeof(struct frame) * user_pages);
}

void *get_free_frame(struct supl_page *page_entry){
	return NULL;
}