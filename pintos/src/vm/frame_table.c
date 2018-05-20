#include "vm/frame_table.h"

void
#include "vm/frame_table.h"

struct frame frame_table[init_ram_pages];

void
frame_table_init ()
{
	return;
}

void *
get_free_frame ()
{
	int i;
	for (i = 0; i < init_ram_pages; i++)
		if (is_free_frame (i))
			return (void *)i;
	ASSERT (false);
	return evict ();
}

bool
is_free_frame (int index)
{
	return (!(frame_table[index].flags & FRAME_OCUPIED));
}

void *
evict () {
	return NULL;
}