#ifndef FRAME_TABLE
#define FRAME_TABLE

#include <round.h>
#include "vm/page_table.h"
#include "threads/vaddr.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "lib/string.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "page_table.h"
#include "../lib/stdbool.h"

// enum frame_flags
//   {
//     FRAME_ACCESSED = 001,
//     FRAME_DIRTY = 002
//   };

struct f_table{
  struct list frame_ls;
  struct lock frame_lock;
  struct frame *frame_ls_array;
};

struct frame
{
	struct supl_page *page;
	struct list_elem ft_elem;
	bool in_use;
};

void frame_table_init (void);

void *get_free_frame(struct supl_page *page_entry);

#endif