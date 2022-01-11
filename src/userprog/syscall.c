#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/usermem.h"
#include "vm/page.h"

#define SYSCALL_ARG_MAX 4

#define SYSCALL_BUFSZ PGSIZE

#define FDTABLE_SIZE ((int) (PGSIZE / sizeof(struct file *)))

typedef uint32_t (*syscall_handler_t) (uint32_t *);
struct syscall_desc
{
  syscall_handler_t handler;
  size_t arg_no;
};

struct lock fs_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init (&fs_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static uint32_t
sys_halt (uint32_t *args UNUSED)
{
  shutdown_power_off ();
}

static uint32_t
sys_exit (uint32_t *args)
{
  int status = args[0];
  struct thread *cur = thread_current ();
  cur->exit_code = status;
  thread_exit ();
}

static uint32_t
sys_exec (uint32_t *args)
{
  struct thread *cur = thread_current ();
  if (!usermem_read_str (cur->syscall_buf, (void *) args[0], SYSCALL_BUFSZ))
    return -1;

  return process_execute (cur->syscall_buf);
}

static uint32_t
sys_wait (uint32_t *args)
{
  return process_wait (args[0]);
}

static uint32_t
sys_create (uint32_t *args)
{
  struct thread *cur = thread_current ();

  /* read the path */
  if (!usermem_read_str (cur->syscall_buf, (void *) args[0], SYSCALL_BUFSZ))
    process_panic ("bad pointer for create");


  lock_acquire (&fs_lock);
  bool result = filesys_create (cur->syscall_buf, args[1]);
  lock_release (&fs_lock);

  return result;
}

static uint32_t
sys_remove (uint32_t *args)
{
  struct thread *cur = thread_current ();

  /* read the path */
  if (!usermem_read_str (cur->syscall_buf, (void *) args[0], SYSCALL_BUFSZ))
    process_panic ("bad pointer for remove");

  lock_acquire (&fs_lock);
  bool result = filesys_remove (cur->syscall_buf);
  lock_release (&fs_lock);

  return result;
}

static uint32_t
sys_open (uint32_t *args)
{
  struct thread *cur = thread_current ();
  int fd;
  struct file *file;

  /* read the path */
  if (!usermem_read_str (cur->syscall_buf, (void *) args[0], SYSCALL_BUFSZ))
    process_panic ("bad pointer for open");

  for (fd = 2; fd < FDTABLE_SIZE; fd++)
    {
      if (cur->fd_table[fd] == NULL)
        break;
    }

  if (fd == FDTABLE_SIZE)
    return -1;

  lock_acquire (&fs_lock);
  file = filesys_open (cur->syscall_buf);
  lock_release (&fs_lock);

  if (file == NULL)
    return -1;

  cur->fd_table[fd] = file;

  return fd;
}

static struct file *
fd_lookup (struct thread *cur, int fd)
{
  if (fd < 2 || fd >= FDTABLE_SIZE)
    return NULL;

  return cur->fd_table[fd];
}

void
fd_close_all (struct thread *t)
{
  int fd;
  for (fd = 2; fd < FDTABLE_SIZE; fd++)
    {
      struct file *file = t->fd_table [fd];
      if (file)
        {
          lock_acquire (&fs_lock);
          file_close (file);
          lock_release (&fs_lock);
        }
    }
}

static uint32_t
sys_filesize (uint32_t *args)
{
  struct thread *cur = thread_current ();
  int fd = args[0];
  struct file *file;

  file = fd_lookup (cur, fd);
  if (file == NULL)
    return -1;

  lock_acquire (&fs_lock);
  off_t len = file_length (file);
  lock_release (&fs_lock);

  return len;
}

static uint32_t
sys_read (uint32_t *args)
{
  int fd = args[0];
  void *user_buf = (void *) args[1];
  unsigned size = args[2];
  struct thread *cur = thread_current ();
  uint8_t *buf = cur->syscall_buf;
  struct file *file;
  if (fd != 0)
    {
      file = fd_lookup (cur, fd);
      if (file == NULL)
        return -1;
    }

  /* we'll do this in chunks of bufsz */
  unsigned off;
  for (off = 0; off < size;) {
    unsigned chunk_size = size - off;
    if (chunk_size > SYSCALL_BUFSZ)
      chunk_size = SYSCALL_BUFSZ;

    unsigned read;
    if (fd == 0)
      {
        *buf = input_getc ();
        read = 1;
      }
    else
      {
        lock_acquire (&fs_lock);
        read = file_read (file, buf, chunk_size);
        lock_release (&fs_lock);
      }

    if (read > 0 && !usermem_write (user_buf + off, buf, read))
      process_panic ("sys_read: reading buffer contents failed\n");

    off += read;
    if (read < chunk_size || fd == 0)
      break;
  }

  return off;
}

static uint32_t
sys_write (uint32_t *args)
{
  int fd = args[0];
  const void *user_buf = (const void *) args[1];
  unsigned size = args[2];
  struct thread *cur = thread_current ();
  void *buf = cur->syscall_buf;
  struct file *file;

  if (fd != 1)
    {
      file = fd_lookup (cur, fd);
      if (file == NULL)
        return -1;
    }

  /* we'll do this in chunks of bufsz */
  unsigned off;
  for (off = 0; off < size;) {
    unsigned chunk_size = size - off;
    if (chunk_size > SYSCALL_BUFSZ)
      chunk_size = SYSCALL_BUFSZ;

    if (!usermem_read (buf, user_buf + off, chunk_size))
      process_panic ("sys_write: reading buffer contents failed\n");

    unsigned written;
    if (fd == 1)
      {
        putbuf ((const char *) buf, chunk_size);
        written = chunk_size;
      }
    else
      {
        lock_acquire (&fs_lock);
        written = file_write (file, buf, chunk_size);
        lock_release (&fs_lock);
      }

    off += written;
    if (written < chunk_size)
      break;
  }

  return off;
}

static uint32_t
sys_seek (uint32_t *args)
{
  struct thread *cur = thread_current ();
  int fd = args[0];
  off_t pos = args[1];
  struct file *file;

  file = fd_lookup (cur, fd);
  if (file == NULL)
    return 0;

  lock_acquire (&fs_lock);
  file_seek (file, pos);
  lock_release (&fs_lock);
  return 0;
}

static uint32_t
sys_tell (uint32_t *args)
{
  struct thread *cur = thread_current ();
  int fd = args[0];
  struct file *file;

  file = fd_lookup (cur, fd);
  if (file == NULL)
    return 0;

  lock_acquire (&fs_lock);
  off_t pos = file_tell (file);
  lock_release (&fs_lock);
  return pos;
}

static uint32_t
sys_close (uint32_t *args)
{
  struct thread *cur = thread_current ();
  int fd = args[0];
  struct file *file;

  file = fd_lookup (cur, fd);
  if (file == NULL)
    return 0;

  lock_acquire (&fs_lock);
  file_close (file);
  lock_release (&fs_lock);

  cur->fd_table[fd] = NULL;
  return 0;
}

struct mapping
  {
    struct list_elem elem;      /* Element. */
    int id;                     /* Mapping id. */
    struct file *file;          /* File. */
    uint8_t *base;              /* Base of the memory mapping. */
    size_t page_cnt;            /* Number of mapped pages. */
  };

struct file_descriptor
  {
    struct list_elem elem;      /* Element. */
    struct file *file;          /* File. */
    int handle;                 /* File handle. */
  };

static struct mapping *
lookup_mapping (int id)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings); e = list_next (e))
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      if (m->id == id)
        return m;
    }

  thread_exit ();
}

static void
unmap (struct mapping *m)
{
  list_remove(&m->elem);

  for(int i = 0; i < m->page_cnt; i++)
  {
    if (pagedir_is_dirty(thread_current()->pagedir, ((const void *) ((m->base) + (PGSIZE * i)))))
    {
      lock_acquire (&fs_lock);
      file_write_at(m->file, (const void *) (m->base + (PGSIZE * i)), (PGSIZE*(m->page_cnt)), (PGSIZE * i));
      lock_release (&fs_lock);
    }
  }

  for(int i = 0; i < m->page_cnt; i++)
  {
    page_deallocate((void *) ((m->base) + (PGSIZE * i)));
  }
}

static int
sys_mmap (int id, void *addr)
{
  struct thread *cur = thread_current ();
  struct file_descriptor *fd = fd_lookup (cur, id);
  struct mapping *m = malloc (sizeof *m);
  size_t offset;
  off_t length;

  if (m == NULL || addr == NULL || pg_ofs (addr) != 0)
    return -1;

  m->id = thread_current ()->next++;
  lock_acquire (&fs_lock);
  m->file = file_reopen (fd->file);
  lock_release (&fs_lock);
  if (m->file == NULL)
    {
      free (m);
      return -1;
    }
  m->base = addr;
  m->page_cnt = 0;
  list_push_front (&thread_current ()->mappings, &m->elem);

  offset = 0;
  lock_acquire (&fs_lock);
  length = file_length (m->file);
  lock_release (&fs_lock);
  while (length > 0)
    {
      struct page *p = page_allocate ((uint8_t *) addr + offset, false);
      if (p == NULL)
        {
          unmap (m);
          return -1;
        }
      
      p->private = false;
      p->file = m->file;
      p->file_offset = offset;
      p->file_bytes = length >= PGSIZE ? PGSIZE : length;
      offset += p->file_bytes;
      length -= p->file_bytes;
      m->page_cnt++;
    }

  return m->id;
}

static int
sys_munmap (int mapping)
{
  struct mapping *map = lookup_mapping(mapping);
  unmap(map);
  return 0;
}

void
syscall_exit (void)
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds); e = next)
    {
      struct file_descriptor *fd = list_entry (e, struct file_descriptor, elem);
      next = list_next (e);
      lock_acquire (&fs_lock);
      file_close (fd->file);
      lock_release (&fs_lock);
      free (fd);
    }

  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings);
       e = next)
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      next = list_next (e);
      unmap (m);
    }
}

static struct syscall_desc syscall_table[] =
  {
    [SYS_HALT] = { sys_halt, 0 },
    [SYS_EXIT] = { sys_exit, 1 },
    [SYS_EXEC] = { sys_exec, 1 },
    [SYS_WAIT] = { sys_wait, 1 },
    [SYS_CREATE] = { sys_create, 2 },
    [SYS_REMOVE] = { sys_remove, 1 },
    [SYS_OPEN] = { sys_open, 1 },
    [SYS_FILESIZE] = { sys_filesize, 1 },
    [SYS_READ] = { sys_read, 3 },
    [SYS_WRITE] = { sys_write, 3 },
    [SYS_SEEK] = { sys_seek, 2 },
    [SYS_TELL] = { sys_tell, 1 },
    [SYS_CLOSE] = { sys_close, 1 },
    [SYS_MAX] = { NULL, 0 },
    [SYS_MMAP] = { sys_mmap, 2 },
    [SYS_MUNMAP] = { sys_munmap, 1 }
  };

static void
syscall_handler (struct intr_frame *f)
{
  uint32_t syscallno;
  struct syscall_desc *d;
  uint32_t args[SYSCALL_ARG_MAX];

  if (!usermem_read_u32 (f->esp, &syscallno))
    process_panic ("syscall_handler: reading syscall number failed\n");

  d = syscall_table + syscallno;
  if (syscallno >= SYS_MAX || d->handler == NULL)
    process_panic ("syscall_handler: invalid syscall number (%u)\n", syscallno);

  size_t i;
  for (i = 0; i < d->arg_no; i++) {
    if (!usermem_read_u32 (f->esp + 4 * (i + 1), &args[i]))
      process_panic ("syscall_handler: reading arg[%zu] failed\n", i);
  }

  f->eax = d->handler (args);
}
