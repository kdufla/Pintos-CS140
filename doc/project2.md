პროექტი #2 - იუზერის პროგრამები: დიზაინ დოკუმენტი
========================================

#### თბილისის თავისუფალი უნივერსიტეტი,  MACS, ოპერაციული სისტემების ინჟინერია, გაზაფხული-2018.

### ჯგუფის წევრები

* <a href="ggvel14@freeuni.edu.ge">გიორგი გველესიანი</a>
* <a href="aabra12@freeuni.edu.ge">ალექსანდრე აბრამიშვილი</a>
* <a href="alkakh14@freeuni.edu.ge">ალექსანდრე კახიძე</a>
* <a href="anaka13@freeuni.edu.ge">ალექსანდრე ნაკაიძე</a>

# 1. Argument Passing
*არგუმენტების გადაცემა*

## მონაცემთა სტრუქტურები და ფუნქციები

### ფუნქციები

* `static uint16_t get_first_token_len(const char *str)` - აბრუნებს გადაცემული სტრინგის პირველი სიტყვის სიგრძეს, ანუ პირველივე ჰარამდე სიმბოლოების რაოდენობას. ვიყენებთ `char`-ების მასივის შესაქმნელად, შემდეგი ფუნქციისთვის.
* `static char *get_first_token(char *dest, const char *src, uint16_t len)` - აბრუნებს და `dest`-ში წერს `src` სტრინგის პირველ `len` რაოდენობის სიმბოლოს. ვიყენებთ სტრინგის პირველი ტოკენის ამოსაღებად, ანუ ამ შემთხვევაში არგუმენტების სიიდან ფაილის სახელის გასაგებად.

## ალგორითმები

ამ ფუნქციონალის იმპლემენტაცია საკმაოდ სწორხაზოვნად იმეორებს დავალების პირობაში მოცემულ მითითებებს:

* ვიღებთ ფაილის სახელისა და არგუმენტებისგან შემდგარ„
 სტრინგ `file_name`-ს, რომელიც `process_execute()` ფუნქციას გადმოეცემა და ვყოფთ ჰარების მიხედვით.
* თითოეულ მიღებულ ელემენტს შებრუნებული თანმიმდევრობით ვწერთ მეხსიერებაში `PHYS_BASE`-იდან დაწყებული კლებადი მიმართულებით.
* შემდეგ ვწერთ საჭიროებისამებრ რამდენიმე 0-ს გასაოთხისჯერადებლად და კიდევ სტანდარტის მიხედვით *null pointer*-ს.
* მერე ვწერთ მეორე ეტაპზე ჩაწერილი თითოეული არგუმენტის მისამართს.
* ბოლოს კი - ამ მისამართებზე მისამართს, არგუმენტების რაოდენობასა და ფსევდო `return address`-ს.

ეს სია მართლა ზედმიწევნით შევასრულეთ, თუ არ ჩავთვლით, კოდის რამდენიმე პატარა ოპტიმიზაციას, მაგალითად იმ ადგილას, სადაც *null pointer*-ი უნდა ჩაგვესვა, იმის გამო, რომ ჩასაწერ მისამართებს `PHYS_BASE`-ზე ვნაშთავდით, არგუმენტების მისამართების ჩაწერის დროს ეს *null pointer*-იც თავის შესაბამის ადგილას ავტომატურად ჩაიწერა. და დავალებაც შესრულდა.

## სინქრონიზაცია

ამ ფუნქციონალს სინქრონიზაცია არ სჭირდება, რადგან ერთი ფუნქციის შიგნით ხდება და ყოველი პროცესისთვის მისი *stack*-ის შევსება ინდივიდუალურია.

## რატომ ავირჩიეთ ეს მიდგომა

დიდად მიდგომის არჩევის საშუალებაც არ მოგვეცა, იმდენად პირდაპირ გავიმეორეთ დავალების პირობაში მოცემული ინსტრუქცია.

# 2. Process Control Syscalls
*პროცესის კონტროლის სისტემური ბრძანებები*

## მონაცემთა სტრუქტურები და ფუნქციები

### სტრუქტურები

* `struct child_info` - ინახავს *exit status*-სა და მისთვის საჭირო სინქრონიზაციის მეთოდებს თითოეული ნაკადისთვის. არსებობს მაშინაც კი, როდესაც ეს ნაკადი მკვდარია.
* `struct file_with_sema` - ინახავს *executable* ფაილის სახელსა და `exec` სისტემური ბრძანებისთვის დასაბრუნებელი მნიშვნელობის სტატუსს.

### ცვლადები

ცვლადები `struct child_info`-ში:

* `tid_t tid` - ნაკადის *id*.
* `int status` - პროცესის *exit status*-ი.
* `bool is_alive` - თუკი ეს ნაკადიც და მშობელიც ორივე ცოცხალია, მაშინ ამ ცვლადის მნიშვნელობა არის `true`, ხოლო ერთი მათგანი მაინც თუ მკვდარია, მაშინ `false`. ვიყენებთ იმის გასარკვევად უსაფრთხო არის თუ არა ამ სტრუქტურის მეხსიერებიდან წაშლა.
* `struct semaphore sema_wait_for_child` - სემაფორა, რომლის გათავისუფლებასაც ელოდება მშობელი *wait*-ის დროს.
* `struct lock *parent_free_lock` - მიმთითებელი მშობელი ნაკადის `free_lock`-ზე.
* `struct list_elem elem` - ელემენტი იმ სიისა, რომელიც აქვს ამ ნაკადის მშობელს.

დამატებითი ცვლადები `struct thread`-ში:

* `struct list child_infos` - შვილი ნაკადების *exit status*-ების *info*-ების სია.
* `struct child_info *info` - საკუთარი *exit status*-ის *info*-ზე მიმთითებელი.
* `struct lock free_lock` - *lock*-ი, რომელიც გამოიყენება შვილების *info*-ების სიაზე მანიპულაციის დროს სინქრონიზაციისთვის.
* `int exit_status` - ამ ნაკადის *exit status*-ი.
* `struct file *executable` - ამ ნაკადის მიერ გაშვებად ფაილზე მიმთითებელი.

ცვლადები `struct file_with_sema`-ში:

* `char *file` - გაშვებადი ფაილის სახელი.
* `struct semaphore *sema` - სემაფორა, რომელიც გამოიყენება `exec` სისტემური ბრძანების დასრულების დალოდებისთვის.
* `bool *status` - `true`-ა იმ შემთხვევაში, თუ ფაილის გაშვება მოხერხდა, ხოლო `false` - საწინააღმდეგო შემთხვევაში.

### ფუნქციები

დამატებითი ფუნქციები `process.c`-ში:

* `struct child_info *get_child_info (struct thread *parent, tid_t child_tid)` - აბრუნებს `parent` ნაკადის `child_tid`-ის მქონე *id*-ით შვილის *exit status*-ის *info*-ს, თუკე ასეთი მოიძებნა.

დამატებითი ფუნქციები `syscall.c`-ში:

* `int practice(int i)` - სისტემური ბრძანება `practice`-ის შემსრულებელი ფუნქცია.
* `static void halt(void)` - სისტემური ბრძანება `halt`-ის შემსრულებელი ფუნქცია.
* `void exit(int status)` - სისტემური ბრძანება `exit`-ის შემსრულებელი ფუნქცია.
* `static pid_t exec(const char *file)` - სისტემური ბრძანება `exec`-ის შემსრულებელი ფუნქცია.
* `static int wait(pid_t pid)` - სისტემური ბრძანება `wait`-ის შემსრულებელი ფუნქცია.
* `static bool is_valid_address(void *p)` - ამოწმებს გადმოცემული მისამართის ვალიდურობას და იმას, გამომძახებელი იუზერის მეხსიერებაში არის თუ არა.
* `static uint32_t *get_arg_int(uint32_t *p)` - ამოწმებს გადმოცემული მიმთითებლის ოთხივე ბაიტის ვალიდურობას.
* `static void *get_arg_pointer(void *p, int len)` - ამოწმებს `p` მისამართზე არსებული მეხსიერების ვალიდურობას, `len`-ის არსებობის შემთხვევაში `len` ბაიტს, თუარადა *null terminator*-ამდე.

## ალგორითმები

სანამ თითოეული სისტემური ბრძანების შესაბამისი ფუნქცია გამოიძახება, `syscall_handler`-იდან ხდება *stack*-ში ჩალაგებული არგუმენტების ამოღება და მათი მეხსიერებების ვალიდურობის შემოწმება.

### Practice

უმატებს გადმოცემულ მთელ რიცხვს ერთს.

### Halt

თიშავს სისტემას.

### Exec

იძახებს `process_execute` ფუნქციას, რომელიც თავის მხრივ ქმნის ახალ ნაკადს, რომელსაც გასაშვებ ფუნქციად აქვს გადაცემული `start_process`-ი, რომელიც უშვებს `exec`-ისთვის გადმოცემული ფაილის სახელის შესაბამის ფაილს.
სწორედ `process_execute`-ის დროს ხდება სემაფორა `sema`-ს ინიციალიზაცია და შემდეგ დალოდება, როდის განთავისუფლდება. ხოლო გაანთავისუფლეს მას შვილი ნაკადის `start_process` ფუნქცია მას შემდეგ რაც გაირკვევა გაეშვა თუ არა პროგრამა.

### Wait

იძახებს `process_wait`-ს, რომელშიც ხდება გადმოცემული *id*-ის მქონე შვილი პროცესის მოძებნა, რომ მას დაელოდოს მშობელი. ელოდება სემაფორა `sema_wait_for_child`-ის მეშვეობით, რომელიც თავისუფლდება მხოლოდ შვილი პროცესის `exit`-ის დროს. აბრუნებს შვილის *exit status*-სა და შლის შვილის `child_info`-ს.

### Exit

იძახებს `process_exit`-ს, რომელშიც გარდა ამ ნაკადისთვის გამოყოფილი მეხსიერების განთავისუფლებისა ხდება დახოცილი შვილების შესაბამისი `struct child_info`-ების წაშლა და ცოცხლებისთვის შეტყობინება, რომ მშობელი გარდაეცვალათ. ასევე თუკი ამ ნაკადის მშობელი მკვდარია, თავისივე *info*-ც იშლება, ხოლო თუ ცოცხალია, ატყობინებს მას, რომ შვილი მოუკვდა. ბოლოს კი იხურება *executable* ფაილი.

## სინქრონიზაცია

ამ დავალებაში სინქრონიზაცია საკმაოდ რთული პროცედურაა. გარდა იმისა, რომ `exec`-ისას შვილის გაშვებასა და `wait`-ისას შვილის `exit`-ს ელოდება მშობელი სწორედ სინქრონიზაციის მეთოდებით, ამ შემთხვევაში `struct semaphore *sema`-თი და `struct semaphore sema_wait_for_child`-ით, იმ დროს, როდესაც ხდება მშობლისა და შვილის კომუნიკაცია, ყოველთვის საჭიროა სინქრონიზაცია, ამიტომ ამ დროს ვიყენებთ მშობელი პროცესის ნაკადის სტრუქტურაში ჩადებულ `free_lock`-ს, რომელზე მიმთითებელიც უნაწილდება ყველა შვილს.

## რატომ ავირჩიეთ ეს მიდგომა

ჩვენი არჩეული მიდგომებით მშობელი და შვილი პროცესები დამოუკიდებლად ახერხებენ მუშაობის გაგრძელებას და რაც მთავარია ბევრ რესურსს არ იყენებენ. მეხსიერებაში ზედმეტი რამდენიმე ათეული ბაიტი იწერება, ხოლო დრო არცერთ ჩვენს მიერ განხორციელებულ სისტემურ ბრძანებას ბევრი არ მიაქვს. ერთადერთი რაც ზომაზე დამოკიდებულია, არის `struct list child_infos`, ანუ შვილების *exit status* ების შემნახველი სტრუქტურების სია, რომელშიც ძებნა ხდება O(n)-ში, თუმცა არც ეს მაჩვენებელი არ ქმნის სისტემისთვის ოდნავ მაინც სახიფათო მდგომარეობას თუმდაც ათასობით შვილი პროცესის შემთხვევაში, რადგან ეს O(n) გადაყოლა ხდება მხოლოდ `wait`-ისა და `exit`-ის დროს, როდესაც მშობელი ან ისედაც ელოდება შვილებს, ან სულაც კვდება და მეტი შესასრულებელი საქმე არ აქვს.

# 3. File Operation Syscalls
*ფაილებზე მოქმედებების სისტემური ბრძანებები*

## მონაცემთა სტრუქტურები და ფუნქციები

### ცვლადები

* `struct lock filesys_lock` - *lock*-ი, რომელიც გამოიყენება ფაილური სისტემის ჩასაკეტად მაშინ, როცა მასზე რამე მანიპულაცია ხდება.
* `#define FD_MAX 128` - ერთი პროცესისთვის მაქსიმალური გახსნილი ფაილების რაოდენობა.

დამატებითი ცვლადები `struct thread`-ში:

* `struct file *descls[FD_MAX]` - მასივი, რომელშიც შენახულია ამ ნაკადის ფაილდესკრიპტორები, ანუ გახსნილი ფაილების იდენტიფიკატორები.

### ფუნქციები

* `bool create(const char *file, unsigned initial_size)` - სისტემური ბრძანება `create`-ის შემსრულებელი ფუნქცია.
* `bool remove(const char *file)` - სისტემური ბრძანება `remove`-ის შემსრულებელი ფუნქცია.
* `int open(const char *file)` - სისტემური ბრძანება `open`-ის შემსრულებელი ფუნქცია.
* `int filesize(int fd)` - სისტემური ბრძანება `filesize`-ის შემსრულებელი ფუნქცია.
* `int read(int fd, void *buffer, unsigned size)` - სისტემური ბრძანება `read`-ის შემსრულებელი ფუნქცია.
* `int write(int fd, const void *buffer, unsigned size)` - სისტემური ბრძანება `write`-ის შემსრულებელი ფუნქცია.
* `void seek(int fd, unsigned position)` - სისტემური ბრძანება `seek`-ის შემსრულებელი ფუნქცია.
* `unsigned tell(int fd)` - სისტემური ბრძანება `tell`-ის შემსრულებელი ფუნქცია.
* `void close(int fd)` - სისტემური ბრძანება `close`-ის შემსრულებელი ფუნქცია.

## ალგორითმები

გარდა იმისა, რომ არსებობს ფაილური სისტემა, რომელშიც შეიძლება ფაილის შექმნა და წაშლა, თუკი ამ ფაილს გახსნის კონკრეტული პროცესი, მის სტუქტურაში ჩაიწერება ფაილდესკრიფტორი, რომელიც მიესადაგება ამ ფაილს.
ფაილდესკრიპტორები ინახება მასივში, რომლის ინდექსი (უფრო სწორად ინდექსს + 2, რადგან 0 და 1 დარეზერვებული `stdin`-ისა და `stdout`-ისათვის) არის სწორედ ეს დესკრიფტორი, ხოლო ამ ინდექსზე წერია მისამართი გახსნილი ფაილის სტრუქტურაზე. დანარჩენი ადგილები სავსეა `NULL`-ებით.

როდესაც ნაკადი ასრულებს მუშაობას მის მიერ გახსნილი ყველა ფაილი იხურება `process_exit` ფუნქციაში.

### Create

ქმნის ახალ ფაილს ფაილურ სისტემაში `filesys_create`-ის გამოძახებით.

### Remove

შლის ფაილს ფაილური სიტემიდან `filesys_remove`-ის გამოძახებით.

### Open

ხსნის ფაილს, თუკი ის არსებობს ფაილურ სისტემაში `filesys_open`-ის გამოძახებით და ფაილდესკრიფტორების მასივის პირველივე არა-`NULL` ადგილას წერს ამ ფაილის სტრუქტურაზე მისამართს.

### Close

ხურავს ფაილს ფაილურ სისტემაში და მის შესაბამის ფაილდესკრიფტორის ადგილას წერს `NULL`-ს. იქამდე ამოწმებს დარეზერვებული დესკრიფტორი ხომ არ შემოუვიდა.

### Filesize

აბრუნებს ფაილის ზომას `file_length` ფუნქციის გამოძახებით. ასევე ამოწმებს დარეზერვებულობაზე.

### Read

წერს გადმოცემული ფაილდესკრიფტორის შესაბამისი ფაილის `size` რაოდენობის ბაიტს ამჟამინდელი კითხვა-ჩაწერის ადგილიდან გადმოცემულ `buffer`-ში. ასევე ამოწმებს დარეზერვებულობაზე და `stdin`-ის შემთხვევაში კითხულობს `input_getc` ფუნქციით.

### Write

წერს გადმოცემული `buffer`-იდან `size` რაოდენობის ბაიტს `fd` ფაილდესკრიფტორის შესაბამისი ფაილის ამჟამინდელ კითხვა-ჩაწერის ადგილას `file_write` ფუნქციის გამოძახებით; ან `stdout`-ის შემთხვევაში - `putbuf`-ის მეშვეობით.

### Seek

ცვლის გადმოცემული ფაილდესკრიფტორის შესაბამისი ფაილის ამჟამინდელ კითხვა-ჩაწერის ადგილს. ასევე არ გამოიყენება დარეზერვებულებისთვის.

### Tell

აბრუნებს გადმოცემული ფაილდესკრიფტორის შესაბამისი ფაილის ამჟამინდელ კითხვა-ჩაწერის ადგილს. ასევე არ გამოიყენება დარეზერვებულებისთვის.

## სინქრონიზაცია

ფაილებთან ოპერაციების ჩატარების დროს გამოიყენება ფაილური სისტემის გლობალური *lock*-ი, რათა არ მოხდეს რამე ასინქრონულობის შეცდომა.

## რატომ ავირჩიეთ ეს მიდგომა

ერთადერთი აშკარა არჩევანი, რაც ამ იმპლემენტაციაში გავაკეთეთ იყო ფაილდესკრიფტორების შენახვის მეთოდი, რომელიც ავირიჩიეთ რომ ყოფილიყო მასივი. მიუხედავად იმისა, რომ მასივი სიცარიელის მიუხედავად იკავებს მუდმივ ადგილს, მისი არჩევის მიზეზი იყო ჩაწერისა და ამოღების სიმარტივე. ჩაწერის შემთხვევაში უახლოეს ცარიელ ადგილს ვიყენებთ, რაც მხოლოდ ყველაზე უარეს შემთხვევაშია O(n), ხოლო ამოღებისას O(1)-ია, რაც სიის შემთხვევაში იქნებოდა O(n), და იქიდან გამომდინარე, რომ ფაილდესკრიფტორით ფაილის მოძებნა ბევრად უფრო ხშირად ხდება, ხოლო ჩაწერა მხოლოდ `open`-ისას, ამიტომ ავირჩიეთ მასივი. გარდა ამისა, იმპლემენტაციაც ბევრად მარტივი გამოვიდა, ვიდრე სიაზე მანიპულაცია გამოვიდოდა.