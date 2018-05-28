პროექტი #3 - ვირტუალური მეხსიერება: დიზაინ დოკუმენტი
========================================

#### თბილისის თავისუფალი უნივერსიტეტი,  MACS, ოპერაციული სისტემების ინჟინერია, გაზაფხული-2018.

### ჯგუფის წევრები

* <a href="ggvel14@freeuni.edu.ge">გიორგი გველესიანი</a>
* <a href="aabra12@freeuni.edu.ge">ალექსანდრე აბრამიშვილი</a>
* <a href="alkakh14@freeuni.edu.ge">ალექსანდრე კახიძე</a>
* <a href="anaka13@freeuni.edu.ge">ალექსანდრე ნაკაიძე</a>

# 1. Page Table Management

## მონაცემთა სტრუქტურები და ფუნქციები

### სტრუქტურები

* `struct f_table` - ~placeholder~
* `struct frame` - ~placeholder~
* `struct supl_page` - ~placeholder~

### ცვლადები

* `struct f_table frame_table` - ~placeholder~

ცვლადები `struct f_table`-ში:

* `struct list frame_ls` - ~placeholder~
* `struct lock frame_lock` - ~placeholder~
* `struct frame *frame_ls_array` - ~placeholder~

ცვლადები `struct frame`-ში:

* `struct supl_page *page` - ~placeholder~
* `struct list_elem ft_elem` - ~placeholder~
* `bool in_use` - ~placeholder~

ცვლადები `struct supl_page`-ში:

* `struct hash_elem hash_elem` - ~placeholder~
* `void *addr` - ~placeholder~
* `struct frame *frame` - ~placeholder~
* `struct file *file` - ~placeholder~
* `off_t ofs` - ~placeholder~
* `uint32_t read_bytes` - ~placeholder~
* `uint32_t zero_bytes` - ~placeholder~
* `bool writable` - ~placeholder~
* `int swapid` - ~placeholder~
* `int mapid` - ~placeholder~

დამატებითი ცვლადი `struct thread`-ში:

* `struct hash pages` - ~placeholder~

### ფუნქციები

ფუნქციები frame_table.c-ში:

* `void frame_table_init (void)` - ~placeholder~
* `void *get_free_frame(struct hash *supl_pages)` - ~placeholder~
* `void remove_frame(struct frame *frame)` - ~placeholder~
* `bool alloc_page(struct supl_page *page, bool load)` - ~placeholder~
* `static void eviction_algorithm(void)` - ~placeholder~
* `bool alloc_page(struct supl_page *page, bool load)` - ~placeholder~
* `static bool install_page_f (void *upage, void *kpage, bool writable)` - ~placeholder~
* `static void evict(uint32_t *pd, struct frame *frame, void *vaddr)` - ~placeholder~

ფუნქციები page_table.c-ში:

* `void set_unalocated_page(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable, int mapid)` - ~placeholder~
* `unsigned page_hash (const struct hash_elem *p_, void *aux)` - ~placeholder~
* `bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux)` - ~placeholder~

## ალგორითმები

*suplemental page table*-ის ინფორმაციაზე წვდომა

ზარმაცი, ანუ მოთხოვნისას დაკმაყოფილებული მეხსიერების ალოკაცია

ერთი *frame*-ის *dirty* და *accessed* ბიტების სინქრონიზება ვირტუალიზებულ ფეიჯებს შორის (კერნელისა და იუზერის ალიასის შემთხვევა)

## სინქრონიზაცია

*frame*-ის გამოყოფის დროს

## რატომ ავირჩიეთ ეს მიდგომა

* Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?

# 2. Paging to and from Disk

## მონაცემთა სტრუქტურები და ფუნქციები

### სტრუქტურები

* `struct swap_map` - ~placeholder~

### ცვლადები

ცვლადები `struct swap_map`-ში:

* `struct lock lock` - ~placeholder~
* `truct bitmap *map` - ~placeholder~

### ფუნქციები

ფუნქციები `swap.c`-ში:

* `void swap_init(void)` - ~placeholder~
* `block_sector_t write_in_swap(void *addr)` - ~placeholder~
* `void read_from_swap(block_sector_t bs, void *addr)` - ~placeholder~

## ალგორითმები

გაძევების ალგორითმი

ერთი პროცესის მიერ მეორე პროცესის ყოფილი *frame*-ის აღების შემთხვევაში რა იცვლება მათ შესაბამის სტრუქტურებში

*stack*-ის გაზრდის ევრისტიკა

## სინქრონიზაცია

* Explain the basics of your VM synchronization design.  In particular, explain how it prevents deadlock.  (Refer to the textbook for an explanation of the necessary conditions for deadlock.)

* A page fault in process P can cause another process Q's frame to be evicted.  How do you ensure that Q cannot access or modify the page during the eviction process?  How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

* Suppose a page fault in process P causes a page to be read from the file system or swap.  How do you ensure that a second process Q cannot interfere by e.g. attempting to evict the frame while it is still being read in?

* Explain how you handle access to paged-out pages that occur during system calls.  Do you use page faults to bring in pages (as in user programs), or do you have a mechanism for "locking" frames into physical memory, or do you use some other design?  How do you gracefully handle attempted accesses to invalid virtual addresses?

## რატომ ავირჩიეთ ეს მიდგომა

* A single lock for the whole VM system would make synchronization easy, but limit parallelism.  On the other hand, using many locks complicates synchronization and raises the possibility for deadlock but allows for high parallelism.  Explain where your design falls along this continuum and why you chose to design it this way.

# 3. Memory Mapped Files

## მონაცემთა სტრუქტურები და ფუნქციები

### სტრუქტურები

* `struct mapel` - ~placeholder~

### ცვლადები

ცვლადები `struct mapel`-ში:

* `void *first` - ~placeholder~
* `void *last` - ~placeholder~
* `struct file *file` - ~placeholder~

დამატებითი ცვლადები `struct thread`-ში:

* `struct mapel maps[MAP_MAX]` - ~placeholder~

### ფუნქციები

* `mapid_t mmap(int fd, void *addr)` - ~placeholder~
* `void munmap(mapid_t)` - ~placeholder~

## ალგორითმები

* Describe how memory mapped files integrate into your virtual memory subsystem.  Explain how the page fault and eviction processes differ between swap pages and other pages.

* Explain how you determine whether a new file mapping overlaps another segment, either at the time the mapping is created or later.

## რატომ ავირჩიეთ ეს მიდგომა

* Mappings created with "mmap" have similar semantics to those of data demand-paged from executables, except that "mmap" mappings are written back to their original files, not to swap.  This implies that much of their implementation can be shared.  Explain why your implementation either does or does not share much of the code for the two situations.
