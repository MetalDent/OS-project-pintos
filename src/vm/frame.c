#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct frame *frames;
static size_t frame_cnt;
static struct lock llock;
static size_t s;

void
frame_init (void) 
{
  void *base_addr;
  lock_init (&llock);
  
  frames = malloc (sizeof *frames * init_ram_pages);
  
  if (frames == NULL)
    PANIC ("Memory full!!");

  while ((base_addr = palloc_get_page (PAL_USER)) != NULL) 
    {
      struct frame *f = &frames[frame_cnt++];
      lock_init (&f->llock);
      f->base_addr = base_addr;
      f->page = NULL;
    }
}

static struct frame *
try_frame_allocate (struct page *page) 
{
  size_t i;
  lock_acquire (&llock);

  for (i = 0; i < frame_cnt; i++)         /* Search for a free frame. */
    {
      struct frame *f = &frames[i];
      if (!lock_try_acquire (&f->llock))
        continue;
      if (f->page == NULL) 
        {
          f->page = page;
          lock_release (&llock);
          return f;
        } 
      lock_release (&f->llock);
    }

  for (i = 0; i < frame_cnt * 2; i++)     /* If there's no free frame. */
    {
      struct frame *f = &frames[s];
      if (++s >= frame_cnt)
        s = 0;

      if (!lock_try_acquire (&f->llock))
        continue;

      if (f->page == NULL) 
        {
          f->page = page;
          lock_release (&llock);
          return f;
        } 

      if (page_accessed_recently (f->page)) 
        {
          lock_release (&f->llock);
          continue;
        }
          
      lock_release (&llock);
      
      if (!page_out (f->page))
        {
          lock_release (&f->llock);
          return NULL;
        }

      f->page = page;
      return f;
    }

  lock_release (&llock);
  return NULL;
}

struct frame *
frame_allocate (struct page *page) 
{
  size_t i;

  for (i = 0; i < 3; i++) 
    {
      struct frame *f = try_frame_allocate (page);
      if (f != NULL) 
        {
          ASSERT (lock_held_by_current_thread (&f->llock));
          return f; 
        }
      timer_msleep (1000);
    }

  return NULL;
}

void
frame_lock (struct page *p) 
{
  struct frame *f = p->frame;
  if (f != NULL) 
    {
      lock_acquire (&f->llock);
      if (f != p->frame)
        {
          lock_release (&f->llock);
          ASSERT (p->frame == NULL); 
        } 
    }
}

void
frame_free (struct frame *f)
{
  ASSERT (lock_held_by_current_thread (&f->llock));
          
  f->page = NULL;
  lock_release (&f->llock);
}

void
frame_unlock (struct frame *f) 
{
  ASSERT (lock_held_by_current_thread (&f->llock));
  lock_release (&f->llock);
}
