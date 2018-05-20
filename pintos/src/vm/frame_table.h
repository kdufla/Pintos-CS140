#include <round.h>
#include "vm/page_table.h"
#include "threads/vaddr.h"
#include "threads/loader.h"
#include "threads/palloc.h"

enum frame_flags
  {
    FRAME_OCUPIED = 001,
    FRAME_ACCESSED = 002,
    FRAME_DIRTY = 004
  };

struct frame
{
	struct page *page;
	int flags;                /* first bit - ocupied, second bit - accessed, third bit - dirty */
};

void frame_table_init (void);