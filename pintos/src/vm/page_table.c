#include "page_table.h"

void set_unalocated_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable);

void set_unalocated_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{

	struct thread *th = thread_current();

	struct supl_page *sp = malloc(sizeof(struct supl_page));
	memset(sp, 0, sizeof(struct supl_page));

	sp->addr = upage;
	sp->file = file;
	sp->ofs = ofs;
	sp->read_bytes = read_bytes;
	sp->zero_bytes = zero_bytes;
	sp->writable = writable;

	hash_insert(&th->pages, &sp->hash_elem);

}
