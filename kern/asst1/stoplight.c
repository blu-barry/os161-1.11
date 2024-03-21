/* 
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
 */


//include
#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>

#define NVEHICLES 30	// num of V

//constants
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

typedef enum TurnDirection {
    R = 0,
	L = 1
} TurnDirection_t;

typedef enum CriticalSection{
	AB = 1,
	BC = 2,
	CA = 4,
	ABnBC = 3,
	ABnCA = 5,
	BCnCA = 6
} CriticalSection_t;

//structs
typedef struct Vehicle {
	// thread_id ?
	unsigned long vehicle_id;
	VehicleType_t vehicle_type;
	Direction_t entrance;
	TurnDirection_t turndirection;
	struct Vehicle* next;
	int critical_section_required
} Vehicle_t;

/*
	A basic queue implementation
	This implementation of a queue does not use dummy head or tail nodes.
	New nodes are added to the tail of the queue. Dequeue removes the node that head points to.
*/
typedef struct Queue {
	Vehicle_t* head;
	Vehicle_t* tail;
	int size;
} Queue_t;

typedef struct MLQ {
	Queue_t A;	// ambulances
	Queue_t C; 	// cars
	Queue_t T; 	// trucks
} MLQ_t;

/* Function Prototypes */

Vehicle_t* create_vehicle(unsigned long vehicle_id, VehicleType_t vehicle_type, Direction_t entrance, TurnDirection_t turndirection);
void free_vehicle(Vehicle_t* v);
int same_vehicle(Vehicle_t* v1, Vehicle_t* v2);
int vehicle_hasNext(Vehicle_t* v);
void print_vehicle(Vehicle_t* v);

Queue_t* Queue_init();
int Queue_isEmpty(Queue_t *q);
int Queue_enqueue(Queue_t *q, Vehicle_t *vehicle);
Vehicle_t* Queue_dequeue(Queue_t *q);
int Queue_free(Queue_t *q);
void queue_extend(Queue_t* receiver, Queue_t* sender);
void display(Queue_t* q);

/* Definitions */
// functions for V 
Vehicle_t* create_vehicle(unsigned long vehicle_id, VehicleType_t vehicle_type, Direction_t entrance, TurnDirection_t turndirection){
	Vehicle_t* v = kmalloc(sizeof(Vehicle_t));
	v->vehicle_id = vehicle_id;
	v->vehicle_type = vehicle_type;
	v->entrance = entrance;
	v->turndirection = turndirection;
	v->critical_section_required=0;
	v->next = NULL;
	return v;
}
void free_vehicle(Vehicle_t* v){
	kfree(v->next);
	kfree(v);
}
int same_vehicle(Vehicle_t* v1, Vehicle_t* v2){
   return v1->vehicle_id == v2->vehicle_id;
}
int vehicle_hasNext(Vehicle_t* v){
	return v->next != NULL;
}
void print_vehicle(Vehicle_t* v){
    printf("Vehicle ID: %lu\n",v->vehicle_id);
	printf("Vehicle Type: %d\n",v->vehicle_type);
	printf(	"Vehicle Direction: %d\n",v->entrance);
	printf("Turn Direction: %d\n",v->turndirection);
	return;
}

/* Queue Functions */

/*  Initializes a queue. 
	returns: The Queue_t* if successful or NULL if it fails.
*/
Queue_t* Queue_init() {
	Queue_t* q = (Queue_t*)kmalloc(sizeof(Queue_t));
	if (q == NULL) { // kmalloc failed
		return NULL;
	}

    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

	return q; // success
}

/* 	Checks if the queue is empty
	returns: The size of the queue if successful or -1 if it fails.
*/
int Queue_isEmpty(Queue_t *q) {
	if (q == NULL) {
		return -1;
	}
    return (q->size == 0);
}

/* Adds a new vehicle to the queue */

/*
	Adds a new node to the tail of the list.
	Returns the new size of the queue if successful, returns -1 if fails.
*/
int Queue_enqueue(Queue_t *q, Vehicle_t *vehicle) {
	if (q == NULL || vehicle == NULL) {
        return 0; // Indicate failure
    }

    vehicle->next = NULL;
    if (q->tail != NULL) {
        q->tail->next = vehicle;
    }
    q->tail = vehicle;
    if (q->head == NULL) {
        q->head = vehicle;
    }
    q->size++;
	return 1; // Indicate success
}

/*	Removes the next vehicle node from the queue.
	returns: The vehicle, or NULL if it fails.

*/
Vehicle_t* Queue_dequeue(Queue_t *q) {
	if (q == NULL || q->head == NULL) {
        return NULL; // The queue is empty, indicate failure to dequeue
    }

	// store the current head of the queue
	Vehicle_t* dequeuedVehicle = q->head;
	q->head = q->head->next;

	// If after removing the head, the queue becomes empty, set the tail to NULL
    if (q->head == NULL) {
        q->tail = NULL;
    }

    q->size--;
	dequeuedVehicle->next = NULL;
	return dequeuedVehicle;
}

/*	Frees a queue and all of the elements it contains.
	returns; 1 if successful, 0 if fails.

*/
int Queue_free(Queue_t *q) {
	if (q == NULL) {
        return 0; // Queue is already NULL
    }

	// Traverse the queue and free each Vehicle_t node
    Vehicle_t* current = q->head;
    while (current != NULL) {
        Vehicle_t* temp = current;
        current = current->next; // Move to the next vehicle before freeing the current one
        free(temp); // Free the memory allocated for the current vehicle
    }

	// After freeing all vehicles, reset the queue's head, tail, and size
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
	kfree(q);
	return 1;
}

// TODO: this functionality will be moved to the MLQ, since the MLQ is what holds the locks
void queue_extend(Queue_t* receiver, Queue_t* sender) { // addeds the sender queue to the receiver queue
	//empty sender
	if(sender->head == NULL){return;}
	//empty reciever
	if(receiver->head == NULL){
		receiver->head = sender->head;
		receiver->tail = sender->tail;
		free_queue(sender);
	}
	//normal situation
	receiver->tail->next = sender->head;
	receiver->tail = sender->tail;
	free_queue(sender);
	return;
} 
void display(Queue_t* q){
	Vehicle_t* cur_v = q->head;
	while(cur_v != NULL){
		print_vehicle(cur_v);
		cur_v = cur_v->next;
	}
}

// TODO; Move these function prototypes later.
//MLQ
MLQ_t* create_mlq();
void free_mlq(MLQ_t* mlq);

// scheduler functions
void consume_waiting_zone(); // consumes the waiting zone mlq
void init_vehicle_scheduler(); // inits scheulder global mlq
void schedule_vehicles();

// waiting zone functions


/*
 * turnright()
 *
 * Arguments:
 *      unsigned long entrance: the direction from which the vehicle
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
/*
input V
based on v->direction, turn->direction
calculate critical section requires.	
*/
static void turnright(Vehicle_t *v)
{
	//calculate critical_section_required
	v->critical_section_required = 2^(v->entrance);	
}

/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long entrance: the direction from which the vehicle
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
static void turnleft(Vehicle_t* v)
{
	int exit;
	//calculate exit
	if(v->entrance == 0){exit = 2;}
	else{exit = v->entrance - 1;}
	//add the second critical section required
	v->critical_section_required = 7-2^(exit);
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
	Direction_t entrance;
	TurnDirection_t turndirection;
	VehicleType_t vehicletype;
	//Avoid unused variable and function warnings. 
	(void) unusedpointer;
	(void) vehiclenumber;
	(void) turnleft;
	(void) turnright;
	//entrance is set randomly.
	entrance = random() % 3;
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
