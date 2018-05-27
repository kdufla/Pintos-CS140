#ifndef PAGE_TABLE
#define PAGE_TABLE

#include "stdint.h"
#include "../lib/stdbool.h"
#include "../lib/kernel/hash.h"
#include "../filesys/off_t.h"
#include "../threads/thread.h"
#include "threads/malloc.h"
#include "lib/string.h"

struct supl_page
{
	struct hash_elem hash_elem; /* Hash table element. */
    void *addr;                 /* Virtual address. */

	// uint32_t *pagedir;
	
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
	
	bool accessed;
	bool dirty;

	int swap_adr;
	int mapid;
	// bool evicted_in_filesys;
};

void set_unalocated_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable);


unsigned page_hash (const struct hash_elem *p_, void *aux);

bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED);
#endif