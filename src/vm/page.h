#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

struct page 
  {
    void *addr;                 /* Virtual address. */
    bool read_only;             /* Read-only */
    struct thread *thread;      /* Thread. */
    struct hash_elem hash_elem; /* Hash element for page. */
    struct frame *frame;        /* Page frame. */
    block_sector_t sector;      
    bool private;               /* Private owned file. */
    struct file *file;          /* File. */
    off_t file_offset;          /* File offset. */
    off_t file_bytes;           /* Bytes to read/write. */
  };

void page_exit (void);
struct page *page_allocate (void *, bool read_only);
void page_deallocate (void *vaddr);
bool page_in (void *fault_addr);
bool page_out (struct page *);
bool page_accessed_recently (struct page *);
bool page_lock (const void *, bool w);
void page_unlock (const void *);

hash_hash_func page_hash;
hash_less_func page_less;

#endif /* vm/page.h */
