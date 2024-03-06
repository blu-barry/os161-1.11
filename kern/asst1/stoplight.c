/* 
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>


/*
 *
 * Constants
 *
 */

/*
 * Number of vehicles created.
 */

#define NVEHICLES 30

typedef enum VehicleType {
	ambulance = 0,
	car = 1,
	truck = 2
} VehicleType_t;

typedef enum Direction {
	A = 0,
	B = 1,
	C = 2
} Direction_t;

typedef struct Vehicle {
	// thread_id ?
	unsigned long vehicle_id;
	VehicleType_t vehicle_type;
	Direction_t vehicledirection;
	Direction_t turndirection;
	struct Vehicle* next;
} Vehicle_t;

typedef struct Queue {
	// TODO implement
} Queue_t;

typedef struct MLQ {
	// TODO implement multi level queue
	Queue_t A;	// ambulances
	Queue_t C; 	// cars
	Queue_t T; 	// trucks
} MLQ_t;

/*
 *
 * Function Definitions
 *
 */
Vehicle_t* create_vehicle(unsigned long vehicle_id, VehicleType_t vehicle_type, Direction_t vehicledirection, Direction_t turndirection){
	Vehicle_t* v = kmalloc(sizeof(Vehicle_t));
	v->vehicle_id = vehicle_id;
	v->vehicle_type = vehicle_type;
	v->vehicledirection = vehicledirection;
	v->turndirection = turndirection;
	v->next = NULL;
	return v;
}

void free_vehicle(Vehicle_t* v);

// queue functions
Queue_t* create_queue();
void free_queue(Queue_t* q);
void enqueue(Vehicle_t * v);
void dnqueue(Vehicle_t * v);
void queue_extend(Queue_t* receiver, Queue_t* sender); // addeds the sender queue to the receiver queue
void display(Queue_t* q);

MLQ_t* create_mlq();
void free_mlq(MLQ_t* mlq);

// scheduler functions
void consume_waiting_zone(); // consumes the waiting zone mlq
void init_vehicle_scheduler(); // inits scheulder global mlq
void schedule_vehicles();

// waiting zone functions


/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long vehicledirection: the direction from which the vehicle
 *              approaches the intersection.
 *      unsigned long vehiclenumber: the vehicle id number for printing purposes.
 * 		unsigned long vehicletype: the vehicle type for priority handling purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a left turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */
static void turnleft(Direction_t vehicledirection, unsigned long vehiclenumber, VehicleType_t vehicletype)
{
	/*
	 * Avoid unused variable warnings.
	 */

	(void) vehicledirection;
	(void) vehiclenumber;
	(void) vehicletype;
}

/*
 * turnright()
 *
 * Arguments:
 *      unsigned long vehicledirection: the direction from which the vehicle
 *              approaches the intersection.
 *      unsigned long vehiclenumber: the vehicle id number for printing purposes.
 * 		unsigned lone vehicletype: the vehicle type for priority handling purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a right turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */
static void turnright(Direction_t vehicledirection, unsigned long vehiclenumber, VehicleType_t vehicletype)
{
	/*
	 * Avoid unused variable warnings.
	 */

	(void) vehicledirection;
	(void) vehiclenumber;
	(void) vehicletype;
}


/*
 * approachintersection()
 *
 * Arguments: 
 *      void * unusedpointer: currently unused.
 *      unsigned long vehiclenumber: holds vehicle id number.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Change this function as necessary to implement your solution. These
 *      threads are created by createvehicles().  Each one is assigned a vehicle type randomly,
 * 	    must choose a direction randomly, approach the intersection, choose a turn randomly, 
 *      and then complete that turn. Making a left or right turn should be done 
 *      by calling one of the functions above.
 */
static void approachintersection(void * unusedpointer, unsigned long vehiclenumber){
	Direction_t vehicledirection, turndirection;
	VehicleType_t vehicletype;

	/*
	 * Avoid unused variable and function warnings.
	 */

	(void) unusedpointer;
	(void) vehiclenumber;
	(void) turnleft;
	(void) turnright;

	/*
	 * vehicledirection is set randomly.
	 */

	vehicledirection = random() % 3;
	turndirection = random() % 2;
	vehicletype = random() % 3;

	// thread has been created

	// create vehicle
	// print state

	// insert into waiting zone MLQ i.e. approached the intersection
	// print state

	// thread sleep

	// turn here after thread wake up occurs

	
}

// scheduler

// thread wake up
// aquires lock
// vehicle thread prints state


/*
 * createvehicles()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up the approachintersection() threads.  You are
 *      free to modify this code as necessary for your solution.
 */
int createvehicles(int nargs, char ** args){
	int index, error;

	/*
	 * Avoid unused variable warnings.
	 */

	(void) nargs;
	(void) args;
	
	// TODO: Set up the scheduler thread

	/*
	 * Start NVEHICLES approachintersection() threads.
	 */

	for (index = 0; index < NVEHICLES; index++) {

		error = thread_fork("approachintersection thread",
				NULL,
				index,
				approachintersection,
				NULL
				);

		/*
		 * panic() on error.
		 */

		if (error) {

			panic("approachintersection: thread_fork failed: %s\n",
					strerror(error)
				 );
		}
	}

	return 0;
}
