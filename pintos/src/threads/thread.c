#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/*Macro for getting max value*/ 
#define max(X, Y)  ((X) > (Y) ? (X) : (Y))

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* List of sleepeing processes. Processes are added to this list
   when they call timer_sleep and removed when the target ticks is reached */
static struct list sleep_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Load average of all threads */
fixed_point_t load_avg;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;
bool is_not_init = false;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority, fixed_point_t recent_cpu, int nice, block_sector_t curdir_inum);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
int priority_in_range (int priority);
void recalculate_load_avg (bool is_idle);
void recalculate_recent_cpu (struct thread *th, void *aux UNUSED);
void recalculate_priority (struct thread *th, void *aux UNUSED);
int thread_calculate_priority (struct thread *th);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
bool priority_list_less_func (const struct list_elem *a, const struct list_elem *b, __attribute__((unused)) void *aux);
void add_thread_to_ready_queue(struct thread *t);
void readd_thread_to_ready_queue(struct thread *t);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init (&sleep_list);

  load_avg = fix_int (0);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT, fix_int (0), 0, 1);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  if (thread_mlfqs && t != idle_thread)
    t->recent_cpu = fix_add (t->recent_cpu, fix_int (1));

  /* When it's multiple of whole seconds */
  if (thread_mlfqs && timer_ticks() % TIMER_FREQ == 0) {
    // enum intr_level old_level = intr_disable ();
    recalculate_load_avg (t == idle_thread);
    thread_foreach (recalculate_recent_cpu, NULL);
    // intr_set_level (old_level);
  }

  thread_wake();
  
  if (++thread_ticks >= TIME_SLICE) {
    if (thread_mlfqs) {
      enum intr_level old_level = intr_disable ();
      thread_foreach (recalculate_priority, NULL);
      intr_set_level (old_level);
    }

    /* Enforce preemption. */
    intr_yield_on_return ();
  }
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority, thread_current ()->recent_cpu, thread_current ()->nice, thread_current ()->curdir_inum);
  tid = t->tid = allocate_tid ();
#ifdef USERPROG
  t->info->tid = tid;
#endif
  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  thread_yield();

  return tid;
}

/* Comparison function of ready threads by priorities.
   Returns true if a is less than or equal to b, false otherwise. */
bool priority_list_less_func (const struct list_elem *a,
                             const struct list_elem *b,
                             __attribute__((unused)) void *aux)
{
  ASSERT(list_entry(a, struct thread, elem)->magic == THREAD_MAGIC);
  ASSERT(list_entry(b, struct thread, elem)->magic == THREAD_MAGIC);
  
  return list_entry(a, struct thread, elem)->priority <= list_entry(b, struct thread, elem)->priority;
}

/* Adds thread to ready list so that priorities of threads
   should be in increasing order */
void add_thread_to_ready_queue(struct thread *t){
  struct list_elem *elem = &t->elem;
  list_insert_ordered(&ready_list, elem, priority_list_less_func, NULL);
}

/* If thread is in ready queue update its place */
void readd_thread_to_ready_queue(struct thread *t){
  if (t->status == THREAD_READY){
    list_remove(&(t->elem));
    add_thread_to_ready_queue(t);
  }
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  add_thread_to_ready_queue(t);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  if(!is_thread (t)){
    ASSERT (is_thread (t));
  }
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    add_thread_to_ready_queue(cur);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the load average of all threads
   based on current value of load average and
   the number of running or ready to run threads */
void
recalculate_load_avg (bool is_idle)
{
  fixed_point_t load_avg_weight = fix_mul (fix_frac (59, 60), load_avg);
  
  /* ready_threads is count of ready to run and running threads
     not including idle thread */
  int ready_threads = list_size (&ready_list) + (is_idle ? 0 : 1);
  
  /* Special case, when initially only idle thread
     is in the ready list we should not count it.
     any other time, it is guaranteed that
     idle will not be included in the ready list */
  if (list_size (&ready_list) == 1 &&
      list_entry (list_front (&ready_list), struct thread, elem) == idle_thread)
    ready_threads = 1;

  fixed_point_t ready_weight = fix_scale (fix_frac (1, 60), ready_threads);
  load_avg = fix_add (load_avg_weight, ready_weight);
}

/* Sets the recent cpu usage to the current thread
   based on the current value of recent cpu usage,
   nice value and load average of all threads*/
void
recalculate_recent_cpu (struct thread *th, void *aux UNUSED)
{
  fixed_point_t load_avg_twice = fix_scale (load_avg, 2);
  fixed_point_t coefficient = fix_div (load_avg_twice, fix_add (load_avg_twice, fix_int (1)));
  th->recent_cpu = fix_add (fix_mul (th->recent_cpu, coefficient), fix_int (th->nice));
}

/* Recalculates priority for the thread th
   and reorders the ready list if the thread was in it. */
void
recalculate_priority (struct thread *th, void *aux UNUSED)
{
  int new_priority = thread_calculate_priority (th);
  if (new_priority != th->priority) {
    th->priority = new_priority;
    readd_thread_to_ready_queue(th);
  }
}

/* Calculates priority of given thread based on mlfqs requirement. */
int
thread_calculate_priority (struct thread *th)
{
  int priority = fix_trunc (fix_sub (fix_int (PRI_MAX), fix_unscale (th->recent_cpu, 4))) - (th->nice * 2);
  return priority_in_range (priority);
}

/* Returns the given number in the range (PRI_MIN:PRI_MAX). */
int
priority_in_range (int priority)
{
  if (priority > PRI_MAX) priority = PRI_MAX;
  if (priority < PRI_MIN) priority = PRI_MIN;
  return priority;
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority)
{
  if (thread_mlfqs) return;
  struct thread *th = thread_current ();
  int actual_priority = priority_in_range (new_priority);
  th->actual_priority = actual_priority;

  struct lock *owned_lock;
  struct list_elem *e, *i;
  int priority = th->actual_priority;

  for(e = list_begin(&th->locks); e != list_end(&th->locks); e = list_next(e)){
    owned_lock = list_entry(e, struct lock, owner_list_elem);
    for(i = list_begin(&owned_lock->semaphore.waiters); i != list_end(&owned_lock->semaphore.waiters); i = list_next(i)){
      priority = max(priority, list_entry(i, struct thread, elem)->priority);
    }
  }
  th->priority = priority;
  readd_thread_to_ready_queue(th);
  thread_yield();
}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE
   and recalculates priority based on it. */
void
thread_set_nice (int nice UNUSED)
{
  struct thread *th = thread_current ();
  th->nice = nice;

  enum intr_level old_level = intr_disable ();
  recalculate_priority (th, NULL);
  intr_set_level (old_level);

  thread_yield();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  /* Not yet implemented. */
  return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  return fix_round (fix_scale (load_avg, 100));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  return fix_round (fix_scale (thread_current ()->recent_cpu, 100));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority, fixed_point_t recent_cpu, int nice, block_sector_t curdir_inum)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);

  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->nice = nice;
  t->recent_cpu = recent_cpu;
  if (thread_mlfqs) {
    priority = thread_calculate_priority (t);
  }
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->waiting_for = NULL;
  t->actual_priority = priority;
  t->magic = THREAD_MAGIC;
  list_init(&t->locks);

  t->curdir_inum = curdir_inum;

#ifdef USERPROG
  /* For userprogs */
  int i;
  for(i = 0; i < FD_MAX; i++){
    t->descls[i] = NULL;
  }
  
  memset(t->maps, 0, sizeof(struct mapel) * MAP_MAX);

  list_init(&t->child_infos);
  lock_init(&(t->free_lock));

  t->executable = NULL;

  if(is_not_init){
    struct child_info *info;
    info = palloc_get_page (PAL_ZERO);
    if (info == NULL)
      return;
      
    memset (info, 0, sizeof *info);

    info->status = 0;
    info->is_alive = true;

    sema_init(&(info->sema_wait_for_child), 0);
    info->parent_free_lock = &(thread_current ()->free_lock);

    t->info = info;

    list_push_back (&(thread_current ()->child_infos), &(info->elem));
  }
  is_not_init = true; 
#endif

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  if (list_empty (&ready_list))
    return idle_thread;
  else{
    return list_entry (list_pop_back (&ready_list), struct thread, elem);
  }
}

/* Comparison function of sleeping threads by their wake times.
   Returns true if a has to be waken earlier than b, false otherwise. */
static bool 
cmp_wake (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct thread *t_a = list_entry(a, struct thread, elem);
  struct thread *t_b = list_entry(b, struct thread, elem);
  return t_a->wake_time < t_b->wake_time;
}
/* Blocks thread untill the time comes to wake up
   given by the variable `untill` */
void 
thread_sleep (int64_t untill)
{
  struct thread *t = thread_current ();
  t->wake_time = untill;
  list_insert_ordered (&sleep_list, &(t->elem), &cmp_wake, NULL);
  thread_block ();
}

/* Unblocks up every thread that should be waken up at this time already */
void 
thread_wake (void)
{
  if (list_empty (&sleep_list)) return;
  int64_t current_time = timer_ticks ();
  
  while ( !(list_empty (&sleep_list)) ){
    struct thread * wake_t = list_entry (list_front (&sleep_list), struct thread, elem);
    if (wake_t->wake_time > current_time)
      break;
    enum intr_level old_level = intr_disable ();
    list_pop_front (&sleep_list);
    thread_unblock (wake_t);
    intr_set_level (old_level);
  }
  return;
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
