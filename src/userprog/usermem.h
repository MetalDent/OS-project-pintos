#ifndef USERPROG_USERMEM_H
#define USERPROG_USERMEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


bool usermem_read (void *, const void *, size_t);
bool usermem_write (void *, const void *, size_t);
bool usermem_read_str (char *, const void *, size_t);

static inline bool
usermem_read_u32 (const void *ptr, uint32_t *ret)
{
  return usermem_read (ret, ptr, 4);
}

#endif /* userprog/usermem.h */
