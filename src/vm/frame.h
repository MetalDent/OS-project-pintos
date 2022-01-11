#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "threads/synch.h"

struct frame 
  {
    struct lock *llock;         /* Lock to prevent access. */
    void *base_addr;            /* Virtual base address. */
    struct page *page;          /* Page. */
  };

void frame_init (void);
struct frame *frame_allocate (struct page *);
void frame_lock (struct page *);
void frame_unlock (struct frame *);
void frame_free (struct frame *);

#endif /* vm/frame.h */
