#ifndef FRAME_TABLE
#define FRAME_TABLE

#include <round.h>
#include "vm/page_table.h"
#include "threads/vaddr.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"

// enum frame_flags
//   {
//     FRAME_ACCESSED = 001,
//     FRAME_DIRTY = 002
//   };

struct f_table{
  struct list frame_ls;
  struct lock frame_lock;
};

struct frame
{
	struct supl_page *page;
	struct list_elem ft_elem;
};

void frame_table_init (void);

void *get_free_frame(enum palloc_flag, supl_page *page_entry);

#endif