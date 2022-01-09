#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

void syscall_init (void);
void fd_close_all (struct thread *);

#endif /* userprog/syscall.h */
