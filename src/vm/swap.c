#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct block *swap_device;
static struct bitmap *swap_bitmap;
static struct lock swap_lock;

#define PAGE_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

void
swap_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  if (swap_device == NULL)
    {
      printf ("No swap device found!!\n");
      swap_bitmap = bitmap_create (0);
    }
  else
    swap_bitmap = bitmap_create (block_size (swap_device) / PAGE_SECTORS);
  
  if (swap_bitmap == NULL)
    PANIC ("PANIC!!");
  
  lock_init (&swap_lock);
}

void
swap_in (struct page *p)
{
  size_t i;

  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->llock));
  ASSERT (p->sector != (block_sector_t) -1);

  for (i = 0; i < PAGE_SECTORS; i++)
    block_read (swap_device, p->sector + i, p->frame->base_addr + i * BLOCK_SECTOR_SIZE);
  bitmap_reset (swap_bitmap, p->sector / PAGE_SECTORS);
  p->sector = (block_sector_t) -1;
}

bool
swap_out (struct page *p)
{
  size_t slot;
  size_t i;

  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->llock));

  lock_acquire (&swap_lock);
  slot = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
  lock_release (&swap_lock);
  
  if (slot == BITMAP_ERROR)
    return false;

  p->sector = slot * PAGE_SECTORS;

  for (i = 0; i < PAGE_SECTORS; i++)
    block_write (swap_device, p->sector + i, (uint8_t *) p->frame->base_addr + i * BLOCK_SECTOR_SIZE);

  p->private = false;
  p->file = NULL;
  p->file_offset = 0;
  p->file_bytes = 0;

  return true;
}
