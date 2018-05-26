#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "syscall.h"

// static struct semaphore temporary;
static thread_func start_process NO_RETURN;
struct child_info *get_child_info (struct thread *parent, tid_t child_tid);
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static uint16_t get_first_token_len(const char *str);
static char *get_first_token(char *dest, const char *src, uint16_t len);

struct file_with_sema{
  char *file;
  struct semaphore *sema;
  bool *status;
};

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  char *fn_copy;
  tid_t tid;
  struct semaphore sema;
  struct file_with_sema fws;
  bool status = true;

  sema_init (&sema, 0);

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  fws.file = fn_copy;
  fws.sema = &sema;
  fws.status = &status;
  

  uint16_t file_name_len = get_first_token_len(file_name);
  char *str_buf[file_name_len + 1];
  char *file_name_ = get_first_token((char *)str_buf, file_name, file_name_len);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name_, PRI_DEFAULT, start_process, &fws);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);

  /* Wait for start_proccess to finish */
  sema_down(&sema);

  return (status ? tid : -1);
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *fws)
{
  char *file_name = ((struct file_with_sema *)fws)->file;
  struct semaphore *sema = ((struct file_with_sema *)fws)->sema;
  bool *status = ((struct file_with_sema *)fws)->status;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success){
    *status = false;
    sema_up(sema);
    thread_exit ();
  }

  *status = true;
  sema_up(sema);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* If child by id child_id exsits return its info, else NULL */ 
struct child_info *
get_child_info (struct thread *parent, tid_t child_tid)
{
  struct list_elem *e;
  struct list *list = &(parent->child_infos);
  for (e = list_begin (list); e != list_end (list); e = list_next (e))
  {
    struct child_info *info = list_entry(e, struct child_info, elem);
    if (info->tid == child_tid)
      return info;
  }
  return NULL;
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  // sema_down (&temporary);
  struct thread *parent = thread_current ();
  lock_acquire (&(parent->free_lock));
  struct child_info *info = get_child_info (parent, child_tid);
  lock_release (&(parent->free_lock));
  if (info == NULL)
    return -1;

  sema_down(&(info->sema_wait_for_child));
  
  lock_acquire (&(parent->free_lock));
  
  int status = info->status;

  list_remove (&(info->elem));
  palloc_free_page (info);

  lock_release (&(parent->free_lock));

  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;


  /* Close all open files */
  lock_acquire(&filesys_lock);  
  int k = 0;
  for(; k < FD_MAX; k++){
    if(cur->descls[k] != NULL){
      file_close(thread_current()->descls[k]);
      cur->descls[k] = NULL;
    }
  }
  lock_release(&filesys_lock);



  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  lock_acquire (&(cur->free_lock));

  struct list_elem *e;
  struct list *list = &(cur->child_infos);
  struct child_info *info;

  /* Deasroy child infos for dead children and inform alive children to destroy their infos */
  for (e = list_begin (list); e != list_end (list); e = list_next (e)){
    info = list_entry(e, struct child_info, elem);
    if (info->is_alive){
      info->is_alive = false;
    } else {
      e = list_prev(list_remove(&(info->elem)));
      palloc_free_page(info);
    }
  }
  lock_release (&(cur->free_lock));
  
  lock_acquire (cur->info->parent_free_lock);

  /* Inform parent that this thread is dying.
    if parent is already dead, destroy chidl_info */
  if (cur->info->is_alive){
    cur->info->is_alive = false;
    cur->info->status = cur->exit_status;
  } else {
    list_remove(&(cur->info->elem));
    palloc_free_page(cur->info);
  }

  lock_release (cur->info->parent_free_lock);

  sema_up(&(cur->info->sema_wait_for_child));
	
  /* Close executable */
  lock_acquire(&filesys_lock);  
  file_close(cur->executable);
  lock_release(&filesys_lock);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char *args_str);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

static uint16_t
get_first_token_len(const char *str)
{
  uint16_t len;
  for (len = 0; len < strlen (str); len++)
    if (str[len] == ' ')
      break;
  return len;
}

static char *
get_first_token(char *dest, const char *src, uint16_t len)
{
  int i;
  for (i = 0; i < len; i++)
    dest[i] = src[i];
  dest[len] = '\0';
  return dest;
}

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  uint16_t file_name_len = get_first_token_len(file_name);
  char *str_buf[file_name_len + 1];
  char *file_name_ = get_first_token((char *)str_buf, file_name, file_name_len);

  hash_init(&t->pages, page_hash, page_less, NULL);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
	lock_acquire(&filesys_lock);
  file = filesys_open (file_name_);
  lock_release(&filesys_lock);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }

  /* Deny write in executable */
	lock_acquire(&filesys_lock);
  file_deny_write(file);
  lock_release(&filesys_lock);

  /* Read and verify executable header. */
	lock_acquire(&filesys_lock);
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      lock_release(&filesys_lock);
      goto done;
    }
  lock_release(&filesys_lock);

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

    	lock_acquire(&filesys_lock);
      if (file_ofs < 0 || file_ofs > file_length (file)){
        lock_release(&filesys_lock);
        goto done;
      }
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr){
        lock_release(&filesys_lock);
        goto done;
      }
      lock_release(&filesys_lock);

      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  // file_close (file);
  if(!success){
  	lock_acquire(&filesys_lock);
    file_close(file);
    lock_release(&filesys_lock);
  }else{
    t->executable = file;
  }

  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
	lock_acquire(&filesys_lock);
  if (phdr->p_offset > (Elf32_Off) file_length (file)){
    lock_release(&filesys_lock);
    return false;
  }
  lock_release(&filesys_lock);

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

	lock_acquire(&filesys_lock);
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      set_unalocated_page(file, ofs, upage, page_read_bytes, page_zero_bytes, writable);

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += page_read_bytes;
    }
  lock_release(&filesys_lock);
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, const char *args_str)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
      {
        *esp = PHYS_BASE;
        int str_len = strlen (args_str);
        char *args[128];
        int num_args = 0;
        bool null_term_needed = true;
        while (str_len > 0)
        {
          str_len--;
          if (args_str[str_len] == ' ')
          {
            null_term_needed = true;
            continue;
          }
          if (null_term_needed)
          {
            null_term_needed = false;

            args[num_args++] = (void *)(((uintptr_t)(*esp)) % ((uintptr_t)PHYS_BASE));
            *((char*)(--(*esp))) = '\0';
          }
          *((char*)(--(*esp))) = args_str[str_len];
        }
        args[num_args++] = (void *)(((uintptr_t)(*esp)) % ((uintptr_t)PHYS_BASE));

        while ((PHYS_BASE - *esp) % 4 != 0)
          *((char*)(--(*esp))) = '\0';

        int i;
        for (i = 0; i < num_args; i++)
        {
          *esp = *esp - sizeof(int);
          *((int*)(*esp)) = (int)args[i];
        }

        int argv = (int)(*esp);
        *esp = *esp - sizeof(int);
        *((int*)(*esp)) = argv;
        *esp = *esp - sizeof(int);
        *((int*)(*esp)) = num_args - 1;
        *esp = *esp - sizeof(int);
        *((int*)(*esp)) = 0;

        // hex_dump ((uintptr_t)(*esp), (void*)(*esp), 64, true);
      }
      else
      {
        palloc_free_page (kpage);
      }
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
