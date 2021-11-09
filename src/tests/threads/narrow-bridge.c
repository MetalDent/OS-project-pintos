/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

#define LEFT 		0
#define RIGHT 	1

int currentDirection = LEFT;
int active = 0;
int waiting = 0; 
int counter = 0;

struct lock lock_res;
struct semaphore space;
struct semaphore mutex; 
struct semaphore direction[2];

void narrow_bridge(unsigned int num_vehicles_left, unsigned int num_vehicles_right,
        unsigned int num_emergency_left, unsigned int num_emergency_right);

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

void BridgeInit(void)							// initiating semaphores
{
  sema_init(&space,3);
  sema_init(&mutex,1);
  sema_init(&direction[LEFT],0);
  sema_init(&direction[RIGHT],0);
}

int Reverse(int direc)							// reverses the direction of the vehicle
{
    if(direc == LEFT)
			return RIGHT;
    return LEFT;
}

void ArriveBridge(int direc, int prio)
{
  int vehicle_num = thread_tid();
  
	printf("Vehicle %d, direction: %d, priority %d Arrived!\n", vehicle_num, direc, prio);
  
	sema_down(&mutex);
  
	if (currentDirection != direc && active > 0)
  {
		printf("Waiting\n");
    waiting++; 											// need to wait
    sema_up(&mutex);
    sema_down(&direction[direc]); 	// when wake up, ready to go!
  } 
  else 
  {
    currentDirection = direc;
    active++;
    sema_up(&mutex);
  }
	
	sema_down(&space); 								// make sure there is space on bridge
}

void CrossBridge(int direc, int prio)
{
  int vehicle_num = thread_tid();
  
	counter++;
  
	printf("Vehicle %d, direction: %d, priority %d, Entered!\n", vehicle_num, direc, prio);
  printf("%d\n", counter);
  
	thread_yield();
}

void ExitBridge(int direc, int prio)
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
      currentDirection = Reverse(direc);
      sema_up(&direction[Reverse(direc)]);
    }
  }
  sema_up(&mutex); 

  int vehicle_num = thread_tid();
  printf("Vehicle %d, direction: %d, priority: %d Exited!\n", vehicle_num, direc, prio);
}

void OneVehicle(int direc)
{
  int prio = thread_get_priority();				// get priority of the vehicle
	
	ArriveBridge(direc, prio);              // arrive at the bridge     
  CrossBridge(direc, prio);               // crossing the bridge 
  ExitBridge(direc, prio);                // exit the bridge          
  
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
  
	for (int i = 0; i < num_vehicles_left; i++) 															// create threads for non-priority left going vehicles
    thread_create("left", 0, OneVehicle, (void *)LEFT);
  
	for (int i = 0; i < num_vehicles_right; i++) 															// create threads for non-priority right going vehicles
    thread_create("right", 0, OneVehicle, (void *)RIGHT);
}
