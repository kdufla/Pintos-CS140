#include "vm/frame_table.h"

static void eviction_algorithm(void);
bool alloc_page(struct supl_page *page, bool load);
static bool install_page_f (void *upage, void *kpage, bool writable);


struct f_table frame_table;

/*
 * Initialize list for fifo
 * Allocate memory for frame entries
 */
void frame_table_init()
{
	memset(&frame_table, 0, sizeof(struct f_table));

	list_init(&frame_table.frame_ls);
	lock_init(&frame_table.frame_lock);

	frame_table.frame_ls_array = malloc(sizeof(struct frame) * user_pages);

	ASSERT(frame_table.frame_ls_array != NULL)

	memset(frame_table.frame_ls_array, 0, sizeof(struct frame) * user_pages);
}

static void evict(uint32_t *pd, struct frame *frame, void *vaddr)
{
	ASSERT (vaddr != NULL);

	void *kpage = pagedir_get_page(pd, vaddr);
	frame->page->swapid = write_in_swap(kpage);
	remove_frame(frame);
	pagedir_clear_page(pd, vaddr);
	palloc_free_page(kpage);
}

static void eviction_algorithm()
{

	// lock_acquire (&frame_table.frame_lock);

	struct frame *frame = list_entry (list_pop_front (&frame_table.frame_ls), struct frame, ft_elem);
	void *vaddr = frame->page->addr;
	uint32_t *pd = frame->page->pagedir;

	while (pagedir_is_accessed (frame->page->pagedir, frame->page->addr)) 
			// || pagedir_is_accessed(frame->page->pagedir, pagedir_get_page(frame->page->pagedir, frame->page->addr)))
	{
		pagedir_set_accessed (frame->page->pagedir, frame->page->addr, false);
		// pagedir_set_accessed(frame->page->pagedir, pagedir_get_page(frame->page->pagedir, frame->page->addr), false);
		list_push_back(&frame_table.frame_ls, &(frame->ft_elem));
		frame = list_entry (list_pop_front (&frame_table.frame_ls), struct frame, ft_elem);
		vaddr = frame->page->addr;
		pd = frame->page->pagedir;
	}

	// lock_release (&frame_table.frame_lock);

	evict (pd, frame, vaddr);
}

void remove_frame(struct frame *frame)
{
	ASSERT(frame != NULL);
	// lock_acquire(&frame_table.frame_lock);
	frame->in_use = false;
	list_remove (&(frame->ft_elem));
	// lock_release(&frame_table.frame_lock);
}


bool alloc_page(struct supl_page *page, bool load)
{
	lock_acquire(&frame_table.frame_lock);

	void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);

	while (kpage == NULL)
	{
		eviction_algorithm();
		kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	}

	size_t i;
	struct frame *frame = NULL;
	// lock_acquire(&frame_table.frame_lock);

	for (i = 0; i < user_pages; i++)
	{
		if (frame_table.frame_ls_array[i].in_use == false)
		{
			frame = &frame_table.frame_ls_array[i];
			frame->in_use = true;
			frame->page = page;
			list_push_back(&frame_table.frame_ls, &frame->ft_elem);
			break;
		}
	}

	page->frame = frame;

	// lock_release(&frame_table.frame_lock);

	/* Load this page. */
	if (load)
	{
		file_seek (page->file, page->ofs);
		if (file_read(page->file, kpage, page->read_bytes) != (int)page->read_bytes)
		{
			palloc_free_page(kpage);
			lock_release(&frame_table.frame_lock);
			return false;
		}
		memset(kpage + page->read_bytes, 0, page->zero_bytes);
	}

	// lock_acquire(&frame_table.frame_lock);

	/* Add the page to the process's address space. */
	if (!install_page_f(page->addr, kpage, page->writable))
	{
		palloc_free_page(kpage);
		lock_release(&frame_table.frame_lock);
		return false;
	}

	pagedir_set_accessed(page->pagedir, page->addr, false);
	pagedir_set_dirty(page->pagedir, page->addr, false);

	// lock_release(&frame_table.frame_lock);	

	if(page->swapid >= 0){
		read_from_swap(page->swapid, page->addr);
		page->swapid = -1;
	}
	lock_release(&frame_table.frame_lock);
	return true;
}

static bool
install_page_f (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  bool get = pagedir_get_page (t->pagedir, upage) == NULL, set = false;

  if(get){
	  set = pagedir_set_page (t->pagedir, upage, kpage, writable);
  }

  return set;

//   /* Verify that there's not already a page at that virtual
//      address, then map our page there. */
//   return (pagedir_get_page (t->pagedir, upage) == NULL
//           && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
