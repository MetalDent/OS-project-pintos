#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "list.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
void *check_address(const void*);
struct process_file *list_search(struct list *files, int fd);

extern bool running;

struct process_file 
{
	struct file* ptr;	      // pointer to the current file
	int fd;			            // file descriptor
	struct list_elem elem;	// element
};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *ptr = f->esp;
	check_address(ptr);

  int system_call = *ptr;
	switch (system_call)
	{
    case SYS_HALT:
		  shutdown_power_off();
		  break;

		case SYS_EXIT:
		  check_address(ptr + 1);
		  exit_process(*(ptr + 1));
		  break;

		case SYS_EXEC:
		  check_address(ptr + 1);
		  check_address(*(ptr + 1));
		  f->eax = execute_process(*(ptr + 1));
		  break;

		case SYS_WAIT:
		  check_address(ptr + 1);
		  f->eax = process_wait(*(ptr + 1));
		  break;

		case SYS_CREATE:
		  check_address(ptr + 5);
		  check_address(*(ptr + 4));
		  acquire_filesys_lock();
		  f->eax = filesys_create(*(ptr + 4),*(ptr + 5));
		  release_filesys_lock();
		  break;

		case SYS_REMOVE:
      check_address(ptr + 1);
		  check_address(*(ptr + 1));
		  acquire_filesys_lock();
		  if(filesys_remove(*(ptr + 1)) == NULL)
        f->eax = false;
		  else
			  f->eax = true;
		  release_filesys_lock();
		  break;

		case SYS_OPEN:
		  check_address(ptr + 1);
		  check_address(*(ptr + 1));

		  acquire_filesys_lock();
		  struct file *fptr = filesys_open(*(ptr + 1));
		  release_filesys_lock();
		  if(fptr == NULL)
			  f->eax = -1;
		  else
		  {
			  struct process_file *pfile = malloc(sizeof(*pfile));
			  pfile->ptr = fptr;
			  pfile->fd = thread_current()->fd_count;
			  thread_current()->fd_count++;
			  list_push_back (&thread_current()->files, &pfile->elem);
			  f->eax = pfile->fd;
		  }
		  break;

		case SYS_FILESIZE:
		  check_address(ptr + 1);
		  acquire_filesys_lock();
		  f->eax = file_length (list_search(&thread_current()->files, *(ptr + 1))->ptr);
		  release_filesys_lock();
		  break;

		case SYS_READ:
		  check_address(ptr + 7);
		  check_address(*(ptr + 6));
		  if(*(ptr + 5) == 0)
		  {
			  int i;
			  uint8_t* buffer = *(ptr + 6);
			  for(i = 0 ; i < *(ptr+7) ; i++)
				  buffer[i] = input_getc();
			  f->eax = *(ptr + 7);
		  }
		  else
		  {
			  struct process_file* fptr = list_search(&thread_current()->files, *(ptr + 5));
			  if(fptr == NULL)
				  f->eax =- 1;
			  else
			  {
				  acquire_filesys_lock();
				  f->eax = file_read (fptr->ptr, *(ptr + 6), *(ptr + 7));
				  release_filesys_lock();
			  }
		  }
		  break;

		case SYS_WRITE:
		  check_address(ptr + 7);
		  check_address(*(ptr + 6));
		  if(*(ptr + 5) == 1)
		  {
			  putbuf(*(ptr + 6), *(ptr + 7));
			  f->eax = *(ptr + 7);
		  }
		  else
		  {
			  struct process_file* fptr = list_search(&thread_current()->files, *(ptr + 5));
			  if(fptr == NULL)
				  f->eax =- 1;
			  else
			  {
				  acquire_filesys_lock();
				  f->eax = file_write (fptr->ptr, *(ptr + 6), *(ptr + 7));
				  release_filesys_lock();
			  }
		  }
		  break;

		case SYS_SEEK:
		  check_address(ptr + 5);
		  acquire_filesys_lock();
		  file_seek(list_search(&thread_current()->files, *(ptr + 4))->ptr,*(ptr + 5));
		  release_filesys_lock();
		  break;

		case SYS_TELL:
		  check_address(ptr + 1);
		  acquire_filesys_lock();
		  f->eax = file_tell(list_search(&thread_current()->files, *(ptr + 1))->ptr);
		  release_filesys_lock();
		  break;

		case SYS_CLOSE:
		  check_address(ptr + 1);
		  acquire_filesys_lock();
		  close_file(&thread_current()->files,*(ptr + 1));
		  release_filesys_lock();
		  break;

		default:
		  printf("Default case block %d\n",*ptr);
	}
}

int execute_process(char *file_name)
{
	acquire_filesys_lock();
	char *fn_cp = malloc(strlen(file_name) + 1);
	strlcpy(fn_cp, file_name, strlen(file_name) + 1);
	  
	char *save_ptr;
	fn_cp = strtok_r(fn_cp, " ", &save_ptr);

	struct file *f = filesys_open(fn_cp);

	if(f == NULL)
	{
	  release_filesys_lock();
	  return -1;
	}
	else
	{
  	file_close(f);
  	release_filesys_lock();
  	return process_execute(file_name);
  }
}

void exit_process(int status)     // method for process exit
{
	struct list_elem *e;

  for (e = list_begin (&thread_current()->parent->child_process) ; e != list_end (&thread_current()->parent->child_process) ; e = list_next (e))
  {
    struct child *f = list_entry(e, struct child, elem);
    if(f->tid == thread_current()->tid)
    {
      f->used = true;
    	f->exit_error = status;
    }
  }

	thread_current()->exit_error = status;

	if(thread_current()->parent->waitingon == thread_current()->tid)
		sema_up(&thread_current()->parent->child_lock);

	thread_exit();
}

void* check_address(const void *vaddr)   // method to validate the pointer address
{
	if (!is_user_vaddr(vaddr))
	{
		exit_process(-1);
		return 0;
	}
  
	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	
  if (!ptr)
	{
		exit_process(-1);
		return 0;
	}
  
	return ptr;
}

struct process_file *list_search(struct list *files, int fd)
{
	struct list_elem *e;

  for (e = list_begin(files) ; e != list_end(files) ; e = list_next(e))
  {
    struct process_file *f = list_entry(e, struct process_file, elem);
    if(f->fd == fd)
      return f;
  }
   return NULL;
}

void close_file(struct list *files, int fd)      // method to close a file
{
	struct list_elem *e;
	struct process_file *f;

  for (e = list_begin(files) ; e != list_end(files) ; e = list_next(e))
  {
    f = list_entry (e, struct process_file, elem);
    if(f->fd == fd)
    {
      file_close(f->ptr);
      list_remove(e);
    }
  }
  
  free(f);
}

void close_all_files(struct list *files)      // method is called to close all files
{
	struct list_elem *e;

	while(!list_empty(files))
	{
		e = list_pop_front(files);

		struct process_file *f = list_entry(e, struct process_file, elem);
          
	  file_close(f->ptr);
	  list_remove(e);
	 	free(f);
	}
}
