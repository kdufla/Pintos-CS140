#include <round.h>
#include "vm/page_table.h"
#include "threads/vaddr.h"
#include "threads/loader.h"
#include "threads/palloc.h"

struct frame
{
	struct page *page;
	int dirty_bits;
};

void frame_table_init (void);