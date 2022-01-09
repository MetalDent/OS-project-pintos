#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/synch.h"
#include "threads/thread.h"

extern struct lock fs_lock;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void process_panic (const char *, ...)  PRINTF_FORMAT (1, 2) NO_RETURN;

#endif /* userprog/process.h */
