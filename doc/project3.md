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

* `struct hash_elem hash_elem` - ჰეშ თეიბლის ელემენტი.
* `void *addr` - ფეიჯის ვირტუალური მისამართი.
* `struct frame *frame` - შესაბამის *frame*-ზე მიმთითებელი.
* `struct file *file` - ფაილზე მიმთითებელი იმ შემთხვევაში თუ ამ ფეიჯში უნდა ეწეროს რაიმე ფაილიდან  (default: NULL)
* `off_t ofs` - ~ ფაილის ოფსეტი.
* `uint32_t read_bytes` - წაკითხული ბაიტების რაოდენობა.
* `uint32_t zero_bytes` - ცარიელი ბაიტების რაოდენობა.
* `bool writable` - შეუძლიათ თუ არა user პროცესებს ამ ფეიჯზე რამის ჩაწერა
* `int swapid` - თუ გატანილია *`swap`*-ში იქაური იდენტიფიკატორი (default: -1)
* `int mapid` - თუ დამეპილია რაიმე ფაილთან შესაბამისი იდენტიფიკატორი (default: -1)

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

* *suplemental page table*-ის ინფორმაციაზე წვდომა.
??????????????

* იუზერის მიერ მოთხოვნილი მეხსიერების ალოკაცია არ ხდება თავიდანვე, არამედ მაშინ როდესაც იუზერი კონკრეტულ ფეიჯს მიმართავს. ეს მიდგომა უპირატესობას გვაძლევს სისწრაფეში და უფრო ოპტიმალურად იყენებს მეხსიერებასაც.

* ერთი *frame*-ის *dirty* და *accessed* ბიტების სინქრონიზება ვირტუალიზებულ ფეიჯებს შორის (კერნელისა და იუზერის ალიასის შემთხვევა) ???????????????

## სინქრონიზაცია

*frame*-ის გამოყოფის დროს იბლოკება კონკრეტული *frame_table* და მასზე ოპერაციის გაკეთება სხვა არცერთ პროცესს არ შეუძლია.

## რატომ ავირჩიეთ ეს მიდგომა

* *Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?*

# 2. Paging to and from Disk

## მონაცემთა სტრუქტურები და ფუნქციები

### სტრუქტურები

* `struct swap_map` - სვეფისთვის საჭირო ინფორმაცია

### ცვლადები

ცვლადები `struct swap_map`-ში:

* `struct lock lock` - ლოქი სინქრონიზაციისთვის
* `truct bitmap *map` - ბიტმეპი თავისუფალი ბლოკების მენეჯმენტისთვის

### ფუნქციები

ფუნქციები `swap.c`-ში:

* `void swap_init(void)` - ლოქის და ბიტმეპის შექმნა
* `block_sector_t write_in_swap(void *addr)` - თავისუფალ `BLOCKS_IN_PAGE` ცალ ბლოკში წერს ფეიჯის კონტენტს
* `void read_from_swap(block_sector_t bs, void *addr)` - სვეფიდან `BLOCKS_IN_PAGE` ბლოკიდან ამოაქვს დეითა მეხსიერებაში 

## ალგორითმები

* გაძევების ალგორითმი

* ერთი პროცესის მიერ მეორე პროცესის ყოფილი *frame*-ის აღების შემთხვევაში რა იცვლება მათ შესაბამის სტრუქტურებში

* *stack*-ის გაზრდის ევრისტიკა

## სინქრონიზაცია

* დედლოქი ვერ მოხდება, რადგან ლოქებს ყოველთვის თანმიმდევრობით ვიღებ, შესაბამისად რეის ქონდიშენი გამორიცხულია

* ერთი პროცესი როცა მეორეს ფრეიმს აძევებს ლოქს არ აბრუნებს, სანამ მოთხოვნილი ფრეიმი არ გამოეყოფა, შესაბამისად, მეორე ვერ მიასწრებს ფეიჯის აღდგენამდე

* ლოუდის დროს პროცესის გაძევება შეუძლებელია, რადგან მასე გაძევება გამორთული აქვს

* გაგდებულ ფეიჯებს ფოლტის დროს აღვადგენ, ხოლო არასწორი მისამართის მოთხოვნის შემთხვევაში პროცესი კვდება -1 სტატუსით

## რატომ ავირჩიეთ ეს მიდგომა

* ერთი ლოქით ვლოქავთ ფრეიმ თეიბლზე წვდომას, როცა გაძევება ან ახალი ფრეიმის გამოყოფა მიმდინარეობს, ხოლო მეორეთ სვოფში ადგილის მოძებნას. მხოლოდ ორი ლოქი საქმეს ზედმეტად არ ართულებს და გაუთვალისწინებელი დედლოქებისგან გვიცავს და ასევე საკმარისი პარალელიზაციის საშუალებას გვაძლევს.

# 3. Memory Mapped Files

## მონაცემთა სტრუქტურები და ფუნქციები

### სტრუქტურები

* `struct mapel` - თითოეული მეპის შესანახი სტრუქტურა

### ცვლადები

ცვლადები `struct mapel`-ში:

* `void *first` - ფაილის საწყისი მისამართი
* `void *last` - ფაილის საბოლოო მისამართი
* `struct file *file` - ფაილის სტრუქტურაზე მიმთითებელი

დამატებითი ცვლადები `struct thread`-ში:

* `struct mapel maps[MAP_MAX]` - დამეპილი ფაილების გლობალური მასივი

### ფუნქციები

* `mapid_t mmap(int fd, void *addr)` - ფუნქცია რომელიც მეპავს ფაილს მეხსიერებაზე იგი უბრალოდ ქმნის საწყის ფეიჯს ასევე *supplemental page table-ში* ავსებს შესაბამის ველებს და ისევე როგორც სხვა მეხსიერებაში ჩაწერის ფუნქციები იყენებს `demand paging`-ს.  
* `void munmap(mapid_t)` - შლის მეპს. ეძებს შესაბამის ფეიჯს თუ არის მეხსიერებაში ასეთი ფეიჯი ამოაგდებს ჰეშ ცხრილიდან, გადაიტანს მის მონაცემებს ისევ ფაილში შესაბამის ადგილზე სადაც აქ უნდა ყოფილიყო და შემდგომ ასუფთავებს ჩვენი ფეიჯის და ფრეიმის მეხსიერებას.

## ალგორითმები

* დამეპილი ფაილები სტრუქტურასთან ინტეგრაციას `supplemental page table`-ს დახმარებით ახერხებენ. იმ შემთხვევაში თუ კონკრეტული ფეიჯი დამეფილია მაშინ მის სტრუქტურაში იწერება არანულოვანი `mapid` რომელიც მიუთითებს გლობალურ მეპებში შესაბამის `mapel` სტრუქტურაზე. შედეგად `page_fault`-ის დროს მიღებული `fault_address`-ით ვიგებთ კონკრეტულ ფეიჯს და ამ ფეიჯიდან ვიღებთ მეპის შესახებ ინფორმაციას. `eviction`-ის დროსაც ანალოგიურად ფეიჯის მისამართის მიხედვით ვიღებთ შესაბამის სტრუქტურას და იმ შემთხევავში თუ `mapid` არის არანულოვანი ეს ფეიჯი გადაგვაქვს ფაილში. ვეძებთ რა ადგილიდან უნდა ჩაიწეროს ფაილში და გადაგვაქვს მონაცემები. შემდგომ თავიდან იგივე ფეიჯზე ჩაწერის შემთხვევაში ფაილის ფეიჯი თავიდან მოიძებნება და თავიდან გადმოვა მეხსიერებაში.

* იმის გასარკვევად კვეთს თუ არა დამეპილი ფაილი სხვა სეგმენტებს 



*Explain how you determine whether a new file mapping overlaps another segment, either at the time the mapping is created or later.*

## რატომ ავირჩიეთ ეს მიდგომა

* *Mappings created with "mmap" have similar semantics to those of data demand-paged from executables, except that "mmap" mappings are written back to their original files, not to swap.  This implies that much of their implementation can be shared.  Explain why your implementation either does or does not share much of the code for the two situations.*
