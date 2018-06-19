პროექტი #4 - ფაილური სისემა: დიზაინ დოკუმენტი
========================================

#### თბილისის თავისუფალი უნივერსიტეტი,  MACS, ოპერაციული სისტემების ინჟინერია, გაზაფხული-2018.

### ჯგუფის წევრები

* <a href="ggvel14@freeuni.edu.ge">გიორგი გველესიანი</a>
* <a href="aabra12@freeuni.edu.ge">ალექსანდრე აბრამიშვილი</a>
* <a href="alkakh14@freeuni.edu.ge">ალექსანდრე კახიძე</a>
* <a href="anaka13@freeuni.edu.ge">ალექსანდრე ნაკაიძე</a>

</br>
</br>

# INDEXED AND EXTENSIBLE FILES

## მონაცემთა სტრუქტურები

ფაილის მაქსიმალური ზომა არის 124 + 128 + 128 * 128 = 16636 ბლოკი


``` c
struct inode_disk
  {
    off_t length;                         /* File size in bytes. */
    block_sector_t direct[DIRECT_BLOCKS]; /* Direct Blocks */
    block_sector_t single;                /* Single Indirect */
    block_sector_t doubly;                /* Double Indirect */
    unsigned magic;                       /* Magic number. */
};
```

``` c
struct block_with_array {
  block_sector_t sectors[ADDS_IN_BLOCK];  /* 128 numbers, 4 byte each (512 bytes or 1 block total) */
};
```

## ფუნქციები

* `block_sector_t *get_memory_on_disk(size_t sectors)` - დისკზე გამოყოფს მოთხოვნილი რაოდენობის ბლოკებს არამიმდევრობით და აბრუნებს ამ ბლოკების მისამართების დინამიურად გამოყოფილ მასივს.


* `void fill_direct_sectors(int sectors,char *zeros, struct inode_disk *disk_inode, block_sector_t *addrs)` - int sectors-მდე ავსებს ნოუდის პირდაპირ მიმართული ბლოკების მასივს გადმოცემული თავისუფალი ბლიკებიის (addrs) ისამართების მასივის მისამართებით.


* `void fill_indirect_sectors(int sectors, char *zeros, int curr, struct block_with_array *sd, block_sector_t *addrs)` - int sectors-მდე ავსებს გადმოცემული ბლოკის ზომის სტრუქტურას (sd) გადმოცემული თავისუფალი ბლიკებიის (addrs) ისამართების მასივის მისამართებით.


* `void release_inode_mem(struct inode_disk *id)` - ათავისუფლებს (unlink) ნოუდის მფლობელობაში მყოფ ყველა ბლოკს.


* `void fill_gap(struct inode_disk *id, size_t off)` off ოფსეტამდე ავსებს ფაილს. თუ ფაილის რეალური ზომა ნაკლებია ოფსეტზე, უცვლის ფაილს ზომას და წერს 0-ებს მოცემულ ზომამდე.


* `static block_sector_t byte_to_sector_create (struct inode *inode, off_t pos)` - აბრუნებს ბლოკ დივაისის სექტორს, რომელშიც წერია ოფსეტი pos. ორიგინალი ფუნქციისგან (byte_to_sector) განსხვავებით, არ არსებობის შემთხვევაში გამოყოფს ბლოკს და იმას აბრუნებს. გამოსადეგია write-სთვის, არაა გამოსადეგი read-სთვის.

---- SYNCHRONIZATION ----

>> A3: Explain how your code avoids a race if two processes attempt to
>> extend a file at the same time.

>> A4: Suppose processes A and B both have file F open, both
>> positioned at end-of-file.  If A reads and B writes F at the same
>> time, A may read all, part, or none of what B writes.  However, A
>> may not read data other than what B writes, e.g. if B writes
>> nonzero data, A is not allowed to see all zeros.  Explain how your
>> code avoids this race.

>> A5: Explain how your synchronization design provides "fairness".
>> File access is "fair" if readers cannot indefinitely block writers
>> or vice versa.  That is, many processes reading from a file cannot
>> prevent forever another process from writing the file, and many
>> processes writing to a file cannot prevent another process forever
>> from reading the file.

## რატომ ავირჩიეთ ეს მიდგომა

ჩვენ ვიყენებთ პირდაპირ, არაპირდაპირ და ორმაგ არაპირდაპირ ინდექსაციის მეთოდებს. ეს მეთოდები ერთად დიდი ზომის ფაილებისთვის არათანმიმდევრული მეხსიერების შენახვის საშუალებას იძლევიან, ხოლო პატარა ზომის ფაილებისთვის მხოლოდ ერთ ბლოკს იყენებენ და მარტივია მენეჯინგისთვის.

# SUBDIRECTORIES

## მონაცემთა სტრუქტურები და ფუნქციები

### ცვლადები

დამატებითი ცვლადი `struct thread`-ში:

* `block_sector_t curdir_inum` - ამ პროცესის მიმდინარე სამუშაო დირექტორიის (current working directory) შესაბამისი inumber-ი.

დამატებითი ცვლადი `struct inode_disk`-ში:

* `bool is_dir` - არის თუ არა ეს inode-ი დირექტორია.

დამატებითი ცვლადი `struct inode`-ში:

* `off_t pos` - პოზიცია, საიდანაც უნდა გაგრძელდეს ამ inode-ის შიგთავსის კითხვა, გამოიყენება `readdir` სიტემური ბრძანების ხელშესაწყობად.

### ფუნქციები

დამატებითი ფუნქციები `syscall.c`-ში:

* `int practice(int i)` - სისტემური ბრძანება `practice`-ის შემსრულებელი ფუნქცია.
* `static void halt(void)` - სისტემური ბრძანება `halt`-ის შემსრულებელი ფუნქცია.
* `void exit(int status)` - სისტემური ბრძანება `exit`-ის შემსრულებელი ფუნქცია.
* `static pid_t exec(const char *file)` - სისტემური ბრძანება `exec`-ის შემსრულებელი ფუნქცია.
* `static int wait(pid_t pid)` - სისტემური ბრძანება `wait`-ის შემსრულებელი ფუნქცია.
* `static bool is_valid_address(void *p)` - ამოწმებს გადმოცემული მისამართის ვალიდურობას და იმას, გამომძახებელი იუზერის მეხსიერებაში არის თუ არა.
* `static uint32_t *get_arg_int(uint32_t *p)` - ამოწმებს გადმოცემული მიმთითებლის ოთხივე ბაიტის ვალიდურობას.
* `static void *get_arg_pointer(void *p, int len)` - ამოწმებს `p` მისამართზე არსებული მეხსიერების ვალიდურობას, `len`-ის არსებობის შემთხვევაში `len` ბაიტს, თუარადა *null terminator*-ამდე.

>> B1: Copy here the declaration of each new or changed `struct` or
>> `struct` member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

ახალი წევრები სტრუქტურებში:

struct thread:
  char *curdir;

struct inode_disk:
  bool is_dir;

## ალგორითმები

>> B2: Describe your code for traversing a user-specified path.  How
>> do traversals of absolute and relative paths differ?

რადგან მომხმარებელს არ ვაძლევთ თავისი ახლანდელი ფოლდერის და არაცარიელი ფოლდერის წაშლის საშუალებას, შეგვიძლია ჩავთვალოთ, რომ: ამ ფოლდერამდე მთლიანი გზა უკვე არსებობდა, შესაბამისად გადაყოლას ვიწყებთ ახლანდელი ფოლდერიდან. მთლიანი path-ის შემთხვევაში გადაყოლას ვიწყებთ root-დან. გზაში თითოეული ფოლდერისთვის მოწმდება რომ ის არსებობს, წინააღმდეგ შემთხვევაში უარყოფითი პასუხით მთავრდება გამოძახება.

რადგან current directory პირდაპირ thread-ის სტრუქტურაშია, chdir-ის დროს უბრალოდ მოწმდება არსებობს თუ არა დირექტორია რომელში გადასვლასაც ცდილობს პროცესი, დადებითი პასუხის შემთხვევაში ისეტება გადმოცემული დირექტორია( /./, /../, // - ებისგან დაცლილი) ნაკადის დირექტორიად.

mkdir - ის დროს მოწმდება რომ არსებობდეს ყველა დირექტორია რაც საბოლოო დირექტორიამდე მისასვლელადაა საჭირო და არ არსებობდეს დირექტორია, რომლის შექმნასაც ვცდილობთ.

open - უკვე მუშაობს ფოლდერებისთვისაც და აღარაა შეზღუდული მხოლოდ root-ში არსებული ფაილების გახსნით.



## სინქრონიზაცია

>> B4: How do you prevent races on directory entries?  For example,
>> only one of two simultaneous attempts to remove a single file
>> should succeed, as should only one of two simultaneous attempts to
>> create a file with the same name, and so on.

ფაილებთან დაკავშირებული race condition-ების გადაჭრა არ არის რთული პროცესი. ფოლდერის წაშლას თუ ცდილობს ორი განსხვავებული ნაკადი, ერთ-ერთს მოუწევს მუტექსზე დალოდება წაშლამდე, რის შემდეგაც ამ ნაკადს ამოუვარდება შეცდომა "ფაილი რომლის წაშლასაც ცდილობთ არ არსებობს". ანალოგიური ვარიანტი გვაქვს იმ შემთხვევაშიც, თუ ორი ნაკადი ცდილობს ფოლდერის შექმნას ერთი და იგივე სახელით.

>> B5: Does your implementation allow a directory to be removed if it
>> is open by a process or if it is in use as a process's current
>> working directory?  If so, what happens to that process's future
>> file system operations?  If not, how do you prevent it?

გადავწყვიტეთ, რომ არ მიგვეცა პროცესებისთვის საშუალება წაეშალათ ისეთი ფოლდერი, რომელიც ახლა ხმარებაშია. ეს საკმაოდ გავრცელებული მიდგომაა, რადგან იგივენაირი ქცევაა linux და windows სისტემებშიც. ეს ნიშნავს იმას, რომ მომხმარებელი მიჩვეულია ხმარებაში მყოფი ფაილების ვერ წაშლას, შესაბამისად არც მომხმარებლის უკმაყოფილებას გამოიწვევს ფოლდერის წაშლის ასეთი იმპლემენტაცია და ჩვენს ბევრად გაგვიმარტივებს საქმეს. საქმე გაგვიმარტივდება, რადგან მხოლოდ ერთი მუტექსის შემოტანით გადავჭერით ხმარებაში მყოფი ფოლდერის წაშლა/არ წაშლის პრობლემა, რომელიც შეიძლება გაპრობლემებულიყო წინააღმდეგ შემთხვევაში.

## რატომ ავირჩიეთ ეს მიდგომა

>> B6: Explain why you chose to represent the current directory of a
>> process the way you did.

ყველაზე პირდაპირი და გამოსადეგი მიდგომა იყო ნაკადის სტრუქტურაში პირდაპირ მთლიანი მისამართის ჩასმა. ასეთნაირად თავიდან ვირიდებთ ნაკადის სტრუქტურაში ზედმეტი ინფორმაციის დამატებას არასაჭირო სტრუქტურების დამატებისგან და ამასთან ერთად გვაქვს საკმაოდ მარტივად წვდომადი/შეცვლადი cwd.

# BUFFER CACHE

## მონაცემთა სტრუქტურები

>> C1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

## ალგორითმები

>> C2: Describe how your cache replacement algorithm chooses a cache
>> block to evict.

>> C3: Describe your implementation of write-behind.

>> C4: Describe your implementation of read-ahead.

## სინქრონიზაცია

>> C5: When one process is actively reading or writing data in a
>> buffer cache block, how are other processes prevented from evicting
>> that block?

>> C6: During the eviction of a block from the cache, how are other
>> processes prevented from attempting to access the block?

## რატომ ავირჩიეთ ეს მიდგომა

>> C7: Describe a file workload likely to benefit from buffer caching,
>> and workloads likely to benefit from read-ahead and write-behind.

			   SURVEY QUESTIONS
			   ================

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

>> In your opinion, was this assignment, or any one of the three problems
>> in it, too easy or too hard?  Did it take too long or too little time?

>> Did you find that working on a particular part of the assignment gave
>> you greater insight into some aspect of OS design?

>> Is there some particular fact or hint we should give students in
>> future quarters to help them solve the problems?  Conversely, did you
>> find any of our guidance to be misleading?

>> Do you have any suggestions for the TAs to more effectively assist
>> students in future quarters?

>> Any other comments?