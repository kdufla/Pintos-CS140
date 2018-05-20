#include "vm/frame_table.h"

void
frame_table_init ()
{
	list_init (&frame_table);
}