#ifndef PAGE_TABLE
#define PAGE_TABLE

#include "stdint.h"

struct supl_page
{
	void *frame_addr;
	uint32_t *pagedir;
	
	// bool accessed;
	// bool dirty;

	int swap_adr;
	// bool evicted_in_filesys;
};

#endif