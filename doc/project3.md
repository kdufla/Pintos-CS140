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

* `struct f_table` - ინახავს *frame table-ს* შესახებ ინფორმაციას.
* `struct frame` - ინახავს ინფორმაციას თითოეული *frame-ს* შესახებ.
* `struct supl_page` - სტრუქტურა რომელიც თითოეული *page-სთვის* ინახავს მისთვის საჭირო ინფორმაციას

### ცვლადები

* `struct f_table frame_table` - მთლიანი *frame table*

ცვლადები `struct f_table`-ში:

* `struct list frame_ls` - *frame*-ების ლისტი გამოიყენება `eviction` დროს `fifo` იმპლემენტაციისთვის.
* `struct lock frame_lock` - *frame table* ლოქი სინქრონიზაციისთვის.
* `struct frame *frame_ls_array` - *frame*-ების მასივი.

ცვლადები `struct frame`-ში:

* `struct supl_page *page` - *`supplementary page-ზე`* მიმთითებელი.
* `struct list_elem ft_elem` - ლისტის ელემენტი.
* `bool in_use` - გამოიყენება თუ არა ეს *`frame`*.

ცვლადები `struct supl_page`-ში:

* `struct hash_elem hash_elem` - ჰეშ ლისტის ელემენტი.
* `void *addr` - რეალური მისამართი.
* `struct frame *frame` - შესაბამის *frame*-ზე მიმთითებელი.
* `struct file *file` - ფაილზე მიმთითებელი იმ შემთხვევაში თუ ამ ფეიჯში უნდა ეწეროს რაიმე ფაილიდან.
* `off_t ofs` - ~ ფაილის ოფსეტი.
* `uint32_t read_bytes` - წაკითხული ბაიტების რაოდენობა.
* `uint32_t zero_bytes` - ცარიელი ბაიტების რაოდენობა.
* `bool writable` - შეუძლიათ თუ არა user პროცესებს ამ ფეიჯზე რამის ჩაწერა
* `int swapid` - თუ გატანილია *`swap`*-ში იქაური იდენტიფიკატორი
* `int mapid` - თუ დამეპილია რაიმე ფაილთან შესაბამისი იდენტიფიკატორი

დამატებითი ცვლადი `struct thread`-ში:

* `struct hash pages` - ჰეშ ცხრილი *`supplementary page table`-ს* შესანახად

### ფუნქციები

ფუნქციები frame_table.c-ში:

* `void frame_table_init (void)` - *`frame table`-ს* ინიციალიზების ფუნქცია.
* `void remove_frame(struct frame *frame)` - *`frame`-ს* გასუფთავების მეთოდი. *`frame`-ს* `in_use` ბულეანს აფოლსებს.
* `bool alloc_page(struct supl_page *page, bool load)` - *`frame`-ს* შექმნის მეთოდი. *`palloc`-ით* იღებს ახალ მისამართს თუ ვერ გამოიყო ცდილობს `eviction`-ს ქმნის ახალ `frame`-ს და უვსებს არსებულ ველებს. თუ ჩასატვირთია ტვირთავს ფაილს და ამატებს ამ `frame`-ს პროცესში.
* `static void eviction_algorithm(void)` - გასაძევებელი ფეიჯის პოვნა. იყენებს `second chance`-ს.
* `static bool install_page_f (void *upage, void *kpage, bool writable)` - რეალური ფეიჯის `frame`-თან შესაბამება
* `static void evict(uint32_t *pd, struct frame *frame, void *vaddr)` - უშუალოდ გამოძევება. იმ შემთხვევაში თუ ფეიჯი არის დამეპილი მაშინ აბრუნებს ამ ფეიჯს ფაილში და შემდეგ ასუფთავებს, ხოლო, თუ არის `swap`-ში გასატანი გააქვს ის ფეიჯები და შემდეგ ასუფთავებს

ფუნქციები page_table.c-ში:

* `void set_unalocated_page(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable, int mapid)` - ცარიელი ფეიჯის გამომყოფი ფუნქცია. 
* `unsigned page_hash (const struct hash_elem *p_, void *aux)` - აბრუნებს ფეიჯის შესაბამის ჰეშ მნიშვნელობას
* `bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux)` - ფეიჯების შესადარებელი ფუნქცია 

## ალგორითმები

* *suplemental page table*-ის ინფორმაციაზე წვდომა

* იუზერის მიერ მოთხოვნილი მეხსიერების ალოკაცია არ ხდება თავიდანვე არამედ მაშინ როდესაც იუზერი კონკრეტულ ფეიჯს მიმართა

* ერთი *frame*-ის *dirty* და *accessed* ბიტების სინქრონიზება ვირტუალიზებულ ფეიჯებს შორის (კერნელისა და იუზერის ალიასის შემთხვევა)

## სინქრონიზაცია

*frame*-ის გამოყოფის დროს

## რატომ ავირჩიეთ ეს მიდგომა

* *Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?*

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

* გაძევების ალგორითმი

* ერთი პროცესის მიერ მეორე პროცესის ყოფილი *frame*-ის აღების შემთხვევაში რა იცვლება მათ შესაბამის სტრუქტურებში

* *stack*-ის გაზრდის ევრისტიკა

## სინქრონიზაცია

* *Explain the basics of your VM synchronization design.  In particular, explain how it prevents deadlock.  (Refer to the textbook for an explanation of the necessary conditions for deadlock.)*

* *A page fault in process P can cause another process Q's frame to be evicted.  How do you ensure that Q cannot access or modify the page during the eviction process?  How do you avoid a race between P evicting Q's frame and Q faulting the page back in?*

* *Suppose a page fault in process P causes a page to be read from the file system or swap.  How do you ensure that a second process Q cannot interfere by e.g. attempting to evict the frame while it is still being read in?*

* *Explain how you handle access to paged-out pages that occur during system calls.  Do you use page faults to bring in pages (as in user programs), or do you have a mechanism for "locking" frames into physical memory, or do you use some other design?  How do you gracefully handle attempted accesses to invalid virtual addresses?*

## რატომ ავირჩიეთ ეს მიდგომა

* *A single lock for the whole VM system would make synchronization easy, but limit parallelism.  On the other hand, using many locks complicates synchronization and raises the possibility for deadlock but allows for high parallelism.  Explain where your design falls along this continuum and why you chose to design it this way.*

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

* *Describe how memory mapped files integrate into your virtual memory subsystem.  Explain how the page fault and eviction processes differ between swap pages and other pages.*

* *Explain how you determine whether a new file mapping overlaps another segment, either at the time the mapping is created or later.*

## რატომ ავირჩიეთ ეს მიდგომა

* *Mappings created with "mmap" have similar semantics to those of data demand-paged from executables, except that "mmap" mappings are written back to their original files, not to swap.  This implies that much of their implementation can be shared.  Explain why your implementation either does or does not share much of the code for the two situations.*
