/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

#define MAX 100                                          // max Buffer Size
#define HELLO "Hello World"                              // constant Hello World String

int buf_count = 0;
int print_count = 0;
int hello_count = 0;
int producer_pos = 0;
int consumer_pos = 0;
char ch;
char buffer[MAX];
char printed[MAX];

struct lock lock;
struct condition empty;
struct condition full;

void producer_consumer(unsigned int num_producer, unsigned int num_consumer);

void test_producer_consumer(void)
{
    // producer_consumer(0, 0);
    // producer_consumer(1, 0);
    // producer_consumer(0, 1);
    // producer_consumer(1, 1);
    // producer_consumer(3, 1);
    // producer_consumer(1, 3);
    // producer_consumer(4, 4);
    // producer_consumer(7, 2);
    // producer_consumer(2, 7);
    producer_consumer(6, 6);
    pass();
}

void producer(void *aux)
{
  while(hello_count < sizeof(HELLO) - 1)
  {
    lock_acquire(&lock);
        
    if(hello_count == sizeof(HELLO) - 1)
    {
      lock_release(&lock);
      return;
    }
    
    while(buf_count == MAX)
      cond_wait(&empty, &lock);
        
    ch = HELLO[hello_count++];
    buffer[producer_pos++ % MAX] = ch;     
    buf_count++;
        
    printf("Producer %d has put char %c in the buffer, now it is: %s \n", thread_tid(), ch, buffer);
        
    cond_signal(&full, &lock); 
    lock_release(&lock);
  }
}

void consumer(void *aux)
{
  while(print_count < sizeof(HELLO) - 1)
  {
    if(print_count == sizeof(HELLO) - 1)
    {
      lock_release(&lock);
      return;
    }
        
    lock_acquire(&lock);
        
    while(buf_count == 0)
      cond_wait(&full,&lock);
        
    ch = buffer[consumer_pos++ % MAX];
    buffer[consumer_pos-1] = ' ';
    printed[print_count++ % MAX] = ch;
    buf_count--;
        
    printf("Consumer %d has taken char %c from the buffer, now it is: %s, printed string: %s \n", thread_tid(), ch, buffer, printed);
        
    cond_signal(&empty,&lock);
    lock_release(&lock);
  }
}

void producer_consumer(UNUSED unsigned int num_producer, UNUSED unsigned int num_consumer)
{
  lock_init(&lock);
  cond_init(&empty);
  cond_init(&full);

  // printf("Size of string: %d \n", sizeof(HELLO));

  for(int i=1;i<=num_producer;i++)
      thread_create("producer", PRI_MIN, producer, (void *)i);
    
  for(int j=1;j<=num_consumer;j++)
      thread_create("consumer", PRI_MIN, consumer, (void *)j);

  // printf("Final Variables: buffercount: %d, printcount: %d, hellocount: %d", buf_count, print_count, hello_count);
}
