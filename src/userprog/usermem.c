#include "userprog/usermem.h"
#include <string.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

/* safely read `len` bytes from user passed address `src` */
bool
usermem_read (void *dst, const void *src, size_t len)
{
  struct thread *t = thread_current ();

  /* copy-by-page if range spans multiple pages, and validate accessibility
     of each page. */
  while (len > 0) {
    size_t pgoff = pg_ofs (src);
    size_t chunk_size = len;
    if (chunk_size > PGSIZE - pgoff)
      chunk_size = PGSIZE - pgoff;

    void *kaddr = pagedir_get_user_valid (t->pagedir, src, false);
    if (!kaddr)
      return false;

    memcpy (dst, kaddr, chunk_size);

    src += chunk_size;
    dst += chunk_size;
    len -= chunk_size;
  }
  return true;
}

/* safely write `len` bytes to user passed address `dst` */
bool
usermem_write (void *dst, const void *src, size_t len)
{
  struct thread *t = thread_current ();

  /* copy-by-page if range spans multiple pages, and validate accessibility
     of each page. */
  while (len > 0) {
    size_t pgoff = pg_ofs (dst);
    size_t chunk_size = len;
    if (chunk_size > PGSIZE - pgoff)
      chunk_size = PGSIZE - pgoff;

    void *kaddr = pagedir_get_user_valid (t->pagedir, dst, true);
    if (!kaddr)
      return false;

    memcpy (kaddr, src, chunk_size);

    src += chunk_size;
    dst += chunk_size;
    len -= chunk_size;
  }

  return true;
}

bool usermem_read_str (char *dst, const void *src, size_t max_len)
{
  struct thread *t = thread_current ();

  /* copy-by-page if range spans multiple pages, and validate accessibility
     of each page. */
  while (max_len > 0) {
    size_t pgoff = pg_ofs (src);
    size_t chunk_size = max_len;
    if (chunk_size > PGSIZE - pgoff)
      chunk_size = PGSIZE - pgoff;

    char *src_s = pagedir_get_user_valid (t->pagedir, src, false);
    if (!src_s)
      return false;

    size_t i;
    for (i = 0; i < chunk_size; i++) {
      *dst++ = src_s[i];
      if (!src_s[i])
        /* yay found a \0 byte */
        return true;
    }

    src += chunk_size;
    max_len -= chunk_size;
  }

  /* no \0 byte found -> error */
  return false;
}