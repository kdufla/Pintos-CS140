#include "vm/frame_table.h"

static void eviction_algorithm(void);
bool load_file_in_page(struct supl_page *page);
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

static void eviction_algorithm()
{
	int out_of_space = 0;
	ASSERT(out_of_space);
}


bool load_file_in_page(struct supl_page *page)
{

	void *kpage = palloc_get_page(PAL_USER & PAL_ZERO);

	while (kpage == NULL)
	{
		eviction_algorithm();
		kpage = palloc_get_page(PAL_USER & PAL_ZERO);
	}

	size_t i;
	struct frame *frame = NULL;
	lock_acquire(&frame_table.frame_lock);

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

	lock_release(&frame_table.frame_lock);

	/* Load this page. */
	if (file_read(page->file, kpage, page->read_bytes) != (int)page->read_bytes)
	{
		palloc_free_page(kpage);
		return false;
	}
	memset(kpage + page->read_bytes, 0, page->zero_bytes);

	/* Add the page to the process's address space. */
	if (!install_page_f(page->addr, kpage, page->writable))
	{
		palloc_free_page(kpage);
		return false;
	}
	return true;
}

static bool
install_page_f (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
