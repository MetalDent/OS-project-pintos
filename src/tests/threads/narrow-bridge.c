/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

// directions
#define LEFT 		0
#define RIGHT 	1

int currentDirection = LEFT;	          // random default direction
int active = 0;								          // no. of active vehicles, can be >3, but actual num will never exceed 3
int waiting = 0; 							          // no. of waiting vehicles

struct semaphore bridge_vehicles;				// semaphore for maintaining max 3 vehicles on the bridge
struct semaphore mutex; 								// semaphore for locking
struct semaphore direction[2];					// semaphore for each direction, used for maintaining direction context

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
	// narrow_bridge(0, 10, 10, 0);
	narrow_bridge(4, 3, 4 ,3);

	pass();
}

void BridgeInit(void)							  // initialising semaphores
{
  sema_init(&bridge_vehicles,3);
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
  
	sema_down(&mutex);			                            // acquiring lock
  
	if (currentDirection != direc && active > 0)				// opposite direction vehicles, got to wait
  {
		// printf("Waiting\n");
    waiting++; 											
    sema_up(&mutex);
    sema_down(&direction[direc]); 	
  } 
  else 																								// can move, set current direction to vehicle direction
  {
    currentDirection = direc;
    active++;
    sema_up(&mutex);
  }
	
	sema_down(&bridge_vehicles); 								        // this is to make sure there is space on the bridge, decrease 1 since entered
}

void CrossBridge(int direc, int prio)
{
  // sema_down(&mutex);
  int vehicle_num = thread_tid(); 
	printf("Vehicle %d, direction: %d, priority %d, Entered!\n", vehicle_num, direc, prio);
	thread_yield();
	printf("Vehicle %d, direction: %d, priority %d, Exited!\n", vehicle_num, direc, prio);
	// sema_up(&mutex);
}

void ExitBridge(int direc, int prio)
{
  // counter--;
  sema_up(&bridge_vehicles);					// since we exited, increase the space available on the bridge
  sema_down(&mutex);
	active--;
  if (active == 0) 										// if the num of active vehicles in this direction is 0, switch directions
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
  // printf("Vehicle %d, direction: %d, priority: %d Exited!\n", vehicle_num, direc, prio);
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
