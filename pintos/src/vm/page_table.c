#include "page_table.h"

void set_unalocated_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable, int mapid);

void set_unalocated_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable, int mapid)
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
	sp->mapid = mapid;

	hash_insert(&th->pages, &sp->hash_elem);

}

 	
/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct supl_page *p = hash_entry (p_, struct supl_page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct supl_page *a = hash_entry (a_, struct supl_page, hash_elem);
  const struct supl_page *b = hash_entry (b_, struct supl_page, hash_elem);

  return a->addr < b->addr;
}
