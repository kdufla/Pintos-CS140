#ifndef FRAME_TABLE
#define FRAME_TABLE

#include <round.h>
#include "vm/page_table.h"
#include "threads/vaddr.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"

enum frame_flags
  {
    FRAME_ACCESSED = 001,
    FRAME_DIRTY = 002
  };

struct list frame_table;

struct frame
{
	struct supl_page *page;
	struct list_elem ft_elem;
	int flags;                /* first bit - accessed, second bit - dirty */
};

void frame_table_init (void);

#endif