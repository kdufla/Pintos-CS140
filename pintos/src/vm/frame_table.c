#include "vm/frame_table.h"


struct f_table frame_table;

void
frame_table_init ()
{
	list_init (&frame_table.frame_ls);
	lock_init(&frame_table.frame_lock);
}

void *get_free_frame(enum palloc_flag, supl_page *page_entry)
{
	void *pg = palloc_get_page(palloc_flags);

	if(pg == NULL){
		// do something
	}

	// do something with page_entry

	return pg;
	
}