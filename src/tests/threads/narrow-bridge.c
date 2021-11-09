/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

#define LEFT 0
#define RIGHT 1
int currentDirection = LEFT;
struct lock lock;
struct semaphore space, mutex, direction[2];
int active = 0, waiting = 0, counter = 0;


void narrow_bridge(unsigned int num_vehicles_left, unsigned int num_vehicles_right,
        unsigned int num_emergency_left, unsigned int num_emergency_right);

int Reverse(int myDir)
{
    if(myDir == LEFT)return RIGHT;
    return LEFT;
}

void test_narrow_bridge(void)
{
    /*narrow_bridge(0, 0, 0, 0);
    narrow_bridge(1, 0, 0, 0);
    narrow_bridge(0, 0, 0, 1);
    narrow_bridge(0, 4, 0, 0);
    narrow_bridge(0, 0, 4, 0);
    narrow_bridge(3, 3, 3, 3);
    narrow_bridge(4, 3, 4 ,3);
    narrow_bridge(7, 23, 17, 1);
    narrow_bridge(40, 30, 0, 0);
    narrow_bridge(30, 40, 0, 0);
    narrow_bridge(23, 23, 1, 11);
    narrow_bridge(22, 22, 10, 10);
    narrow_bridge(0, 0, 11, 12);
    narrow_bridge(0, 10, 0, 10);*/
    narrow_bridge(0, 10, 10, 0);
    pass();
}

void BridgeInit(void)
{
    sema_init(&space,3);
    sema_init(&mutex,1);
    sema_init(&direction[LEFT],0);
    sema_init(&direction[RIGHT],0);
}

// static int isSafe(int Direction)
// {
//   if (vehicles == 0)                // check if no vehicle on bridge
//     return  TRUE;                   // then it's safe to cross            
//  else if ((vehicles < MAX_VEHICLE) && (cur_direc == Direction))
//    return  TRUE;                    // if < 3 in same direction 
//   else
//     return  FALSE;                  // do not proceed
// }

void ArriveBridge(int myDir)
{
    int vehicle_num = thread_tid();
    int priority = thread_get_priority();
    printf("Vehicle %d, direction: %d, priority %d Arrived!\n", vehicle_num, myDir, priority);
    sema_down(&mutex);
    if (currentDirection != myDir && active > 0)
    {
        printf("Waiting\n");
        waiting++; // need to wait
        sema_up(&mutex);
        sema_down(&direction[myDir]); //when wake up, ready to go!
    } 
    else 
    {
        currentDirection = myDir;
        active++;
        sema_up(&mutex);
    }
    sema_down(&space); // make sure there is space on bridge
}

void CrossBridge(int myDir)
{
    int priority = thread_get_priority();
    int vehicle_num = thread_tid();
    counter++;
  printf("Vehicle %d, direction: %d, priority %d, Entered!\n", vehicle_num, myDir, priority);
  printf("%d\n", counter);
  thread_yield();
}

void ExitBridge(int myDir)
{
    counter--;
    sema_up(&space);
    sema_down(&mutex);
    if (active-- == 0) 
    {
        while (waiting > 0) 
        {
            waiting--;
            active++;
            currentDirection = Reverse(myDir);
            sema_up(&direction[Reverse(myDir)]);
        }
    }
    sema_up(&mutex); 
    int priority = thread_get_priority();
    int vehicle_num = thread_tid();
    printf("Vehicle %d, direction: %d, priority: %d Exited!\n", vehicle_num, myDir, priority);
}

void OneVehicle(int myDir)
{
  ArriveBridge(myDir);              // arrive at the bridge     
  CrossBridge(myDir);               // crossing the bridge 
  ExitBridge(myDir);                // exit the bridge          
  thread_exit();
}

void narrow_bridge(UNUSED unsigned int num_vehicles_left, UNUSED unsigned int num_vehicles_right,
        UNUSED unsigned int num_emergency_left, UNUSED unsigned int num_emergency_right)
{
  BridgeInit();
  
  if (num_emergency_left > num_emergency_right)                             // check if left emergency vehicles are more
  {
    for (int i = 0; i < num_emergency_left; i++)   
      thread_create("left emergency", 1, OneVehicle, (void *)LEFT);         // create threads for left direction with prio=1     
    
    if (num_emergency_right)                                                // if there are any right side emergency vehicle
    {
      for (int i = 0; i < num_emergency_right; i++) 
        thread_create("right emergency", 1, OneVehicle, (void *)RIGHT);     // create threads for right direction with prio=1
    }
  }
  else                                                                      // if no of right emergency vehicles are more 
  {
    for (int i = 0; i < num_emergency_right; i++) 
      thread_create("right emergency", 1, OneVehicle, (void *)RIGHT);
    
    if (num_emergency_left)
    {
      for (int i = 0; i < num_emergency_left; i++) 
        thread_create("left emergency", 1, OneVehicle, (void *)LEFT);
    }      
  }
  
    for (int i = 0; i < num_vehicles_left; i++) 
      thread_create("left", 0, OneVehicle, (void *)LEFT);
    for (int i = 0; i < num_vehicles_right; i++) 
      thread_create("right", 0, OneVehicle, (void *)RIGHT);
}
