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
#include <synch.h>
#include <stdio.h>
#include <stdlib.h>

#define NVEHICLES 30	// num of V

/* Constants */

typedef enum {
    SUCCESS = 0,                        // Operation successful
    ERROR_NULL_POINTER = -1,            // Null pointer passed to the function
	ERROR_LOCK_ACQUIRE_FAILED = -2,     // Failed to acquire a necessary lock
	ERROR_LOCK_RELEASE_FAILED = -3,     // Failed to release a lock (if applicable)
    ERROR_LOCK_DESTROY_FAILED = -4,     // Failed to destroy the lock
    ERROR_QUEUE_FULL = -5,              // The queue is full (for fixed-size queues)
    ERROR_QUEUE_EMPTY = -6,             // The queue is empty, nothing to consume
    ERROR_MEMORY_ALLOCATION_FAILED = -7,// Memory allocation failed
    ERROR_INVALID_OPERATION = -8        // Invalid operation attempted
} StoplightError;

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

/* 	A struct representing a vehicle
	This is the node type used for the queue.
*/
typedef struct Vehicle {
	unsigned long  vehiclenumber;						// Unsigned long since it is based on NVEHICLES. The address the vehicle thread sleeps on when it enters the waiting_zone. Eventually the scheduler will wake it up via this address.
	VehicleType_t vehicle_type;
	Direction_t entrance;
	TurnDirection_t turndirection;
	lock_t* lock;										// used in the hand over hand locking mechanism
	struct Vehicle* next;
	int intersection_segment_required;				
} Vehicle_t;

/*	A basic queue implementation
	This implementation of a queue does not use dummy head or tail nodes.
	New nodes are added to the tail of the queue. Dequeue removes the node that head points to.
*/
typedef struct Queue {
	Vehicle_t* head;
	Vehicle_t* tail;
	int size;
} Queue_t;

// A multilevel queue that is used for both the scheduler and the waiting zone. Contains multiple queues that contain vehicles
typedef struct MLQ {
	Queue_t* A;			// ambulances
	Queue_t* C; 		// cars
	Queue_t* T; 		// trucks
	lock_t* lockA; 		// queue A lock
	lock_t* lockC;		// queue C lock
	lock_t* lockT;		// queue T lock
	int sleepAddr;		// a generic value that is used in the scheduler to sleep on, ultimately allowing other threads to wake it up.
} MLQ_t;

/* Function Prototypes */

Vehicle_t* create_vehicle(int vehiclenumber, VehicleType_t vehicle_type, Direction_t entrance, TurnDirection_t turndirection);
const char* createVehicleLockNameString(unsigned long lockNumber);
int Vehicle_free(Vehicle_t* vehicle);
void free_vehicle(Vehicle_t* v); 		// TODO: REMOVE THIS LATER
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

MLQ_t* mlq_init();

/* Global Variables */
MLQ_t* vehicle_scheduler;
MLQ_t* waiting_zone;

// exited vehicles counter and lock
int numExitedV;
lock_t* numExitedVLock;


// Intersection segment locks. NOTHING IS ALLOWED TO TOUCH THESE BY THE SCHEDULER AND WHICHEVER VEHICLE THREAD IT ALLOWS
lock_t* isegAB_lock;
lock_t* isegBC_lock;
lock_t* isegCA_lock;

/* Definitions */
// functions for V 
Vehicle_t* create_vehicle(int vehiclenumber, VehicleType_t vehicle_type, Direction_t entrance, TurnDirection_t turndirection) {
	Vehicle_t* v = kmalloc(sizeof(Vehicle_t));
	if(v==NULL){
		return NULL;
	}
	v->vehiclenumber = vehiclenumber;
	v->vehicle_type = vehicle_type;
	v->entrance = entrance;
	v->turndirection = turndirection;
	const char* lockName = createVehicleLockNameString(vehiclenumber);
	if (lockName == NULL) {
		free_vehicle(v);
		return NULL;
	}
	v->lock = lock_create(lockName);
	v->intersection_segment_required = 0;
	v->next = NULL;
	return v;
}

const char* createVehicleLockNameString(unsigned long lockNumber) {
    // Prefix string
    const char* prefix = "Vehicle Lock Number: ";
    // Calculate the length needed for the unsigned long as a string
    // +1 for the null terminator
    int numberLength = snprintf(NULL, 0, "%lu", lockNumber) + 1;
    // Calculate total length: prefix length + number length + null terminator
    int totalLength = snprintf(NULL, 0, "%s%lu", prefix, lockNumber) + 1;
    
    // Dynamically allocate memory for the full string
    char* fullString = (char*)malloc(totalLength * sizeof(char));
    if (fullString == NULL) {
        // Memory allocation failed
        return NULL;
    }

    // Construct the full string
    snprintf(fullString, totalLength, "%s%lu", prefix, lockNumber);
    
    // Return the dynamically allocated full string
    // Caller is responsible for freeing this memory
    return fullString;
}

int Vehicle_free(Vehicle_t* vehicle) {
	if (vehicle == NULL) {
        return ERROR_NULL_POINTER;
    }

	if (vehicle->lock != NULL) {
        lock_destroy(vehicle->lock); // Assume success
        vehicle->lock = NULL;
    }

	kfree(vehicle);

    return SUCCESS; // Indicate success
}

// TODO Safely free the vehicle regardless of what fields are initialized
void free_vehicle(Vehicle_t* v){ // TODO: THIS FUNCTION IS WRONG AS WELL. WHY DOES IS IT FREE BOTH THIS VEHICLE AND NEXT? This results in an error. It should ensure that the next pointer is null first. otherwise a pointer to the next vehicle may be lost
	if (v->next != NULL) {
		kfree(v->next);
	}
	if (v != NULL) {
		kfree(v);
	}
}

int same_vehicle(Vehicle_t* v1, Vehicle_t* v2){
   return v1->vehiclenumber == v2->vehiclenumber;
}

int vehicle_hasNext(Vehicle_t* v){
	return v->next != NULL;
}

void print_vehicle(Vehicle_t* v){
    printf("Vehicle ID: %lu\n",v->vehiclenumber);
	printf("Vehicle Type: %d\n",v->vehicle_type);
	printf("Vehicle Direction: %d\n",v->entrance);
	printf("Turn Direction: %d\n",v->turndirection);
	return;
}

/* Queue Functions */

/*  Initializes a queue with a dummy node.
	returns: The Queue_t* if successful or NULL if it fails.
*/
Queue_t* Queue_init() {
    Queue_t* q = (Queue_t*)kmalloc(sizeof(Queue_t));
    if (q == NULL) { // kmalloc failed
        return NULL;
    }
    
    // Create a dummy node
    Vehicle_t* dummy = (Vehicle_t*)kmalloc(sizeof(Vehicle_t));
    if (dummy == NULL) { // kmalloc failed for dummy
        kfree(q); // Clean up previously allocated queue
        return NULL;
    }
    dummy->next = NULL;
    // Dummy node does not require valid vehicle data

    q->head = dummy; // Head points to dummy
    q->tail = dummy; // Tail also points to dummy initially
    q->size = 0; // Queue size is 0

    return q; // success
}

/* Adds a new node to the tail of the list, after the dummy node. */
int Queue_enqueue(Queue_t *q, Vehicle_t *vehicle) {
    if (q == NULL || vehicle == NULL) {
        return ERROR_NULL_POINTER; // Indicate failure
    }

    vehicle->next = NULL;
    q->tail->next = vehicle; // Link new vehicle to the end of the queue
    q->tail = vehicle; // Update tail to new vehicle
    if (q->size == 0) {
        q->head->next = vehicle; // If queue was empty, point head's next to new vehicle
    }
    q->size++;
    return q->size; // Return new size of the queue
}

/* Removes the node after the dummy node from the queue. */
Vehicle_t* Queue_dequeue(Queue_t *q) {
    if (q == NULL || q->size == 0) {
        return NULL; // The queue is empty or invalid, indicate failure to dequeue
    }

    Vehicle_t* dequeuedVehicle = q->head->next; // The vehicle to dequeue
    q->head->next = dequeuedVehicle->next; // Remove dequeued vehicle from chain

    if (q->head->next == NULL) {
        q->tail = q->head; // If queue becomes empty, reset tail to dummy
    }

    q->size--;
    dequeuedVehicle->next = NULL; // Prevent potential dangling pointer
    return dequeuedVehicle;
}

/*	Frees a queue and all of the elements it contains.
	returns; 1 if successful, 0 if fails.

*/
int Queue_free(Queue_t *q) {
    if (q == NULL) {
        return ERROR_NULL_POINTER; // Queue is already NULL, indicating failure
    }

    // Start with the first real node, skipping the dummy node
    Vehicle_t* current = q->head->next;
    while (current != NULL) {
        Vehicle_t* temp = current;
        current = current->next; // Move to the next vehicle before freeing the current one
        kfree(temp); // Free the memory allocated for the current vehicle
    }

    // After freeing all vehicles, free the dummy node itself
    // The dummy node is pointed to by q->head
    kfree(q->head);

    // After freeing all vehicles and the dummy node, free the queue structure itself
    kfree(q);

    return SUCCESS; // Indicate success
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

// TODO: this is certainly not thread safe
void display(Queue_t* q){ // TODO: should we implement read write, or hand over hand locking to allow multiple readers at once? This may help with I/O. I know this does not need to be a thread safe function but when the function is called
	Vehicle_t* cur_v = q->head;
	while(cur_v != NULL){
		print_vehicle(cur_v);
		cur_v = cur_v->next;
	}
}

//MLQ
MLQ_t* mlq_init(){
	MLQ_t* mlq = (MLQ_t*)kmalloc(sizeof(MLQ_t));
	if(mlq==NULL){ return NULL; }

	mlq->A = create_queue();
	if (mlq->A == NULL) {
		kfree(mlq);
		return NULL;
	}

	mlq->C = create_queue();
	if (mlq->C == NULL) {
		Queue_free(mlq->A);
		kfree(mlq);
		return NULL;
	}

	mlq->T = create_queue();
	if (mlq->T == NULL) {
		Queue_free(mlq->A);
		Queue_free(mlq->C);
		kfree(mlq);
		return NULL;
	}
	// TODO: What should the lock names be?
	const char *Aname = "mlq_lock_A"; // TODO: Make sure that this does not cause a memory leak
	const char *Cname = "mlq_lock_C";
	const char *Tname = "mlq_lock_T";
	mlq->lockA = lock_create(Aname);
	mlq->lockC = lock_create(Cname);
	mlq->lockT = lock_create(Tname);
	mlq->sleepAddr = 0;
	return mlq;
}

void free_mlq(MLQ_t* mlq){ // TODO: NULL Pointer checks are needed here
	free_queue(mlq->A);
	free_queue(mlq->C);
	free_queue(mlq->T); // TODO: Does free_queue and lock_destroy do NULL checks?
	lock_destroy(mlq->lockA);
	lock_destroy(mlq->lockC);
	lock_destroy(mlq->lockT);
	kfree(mlq);
	return;
}

// TODO: this does not appear to be thread safe
void print_state(MLQ_t* mlq){
	print("A:\n");
	display(mlq->A);
	print("C:\n");
	display(mlq->C);
	print("T:\n");
	display(mlq->T);
	return;
}

// scheduler functions

// TODO: implement hand of hand locking when consuming the waiting zone
// need to acquire all of the locks for a turn
//absorb v from wait zone and leave an empty body
void consume_waiting_zone(MLQ_t* wait_zone, MLQ_t* scheduler_mlq){ // TODO: How is the waiting zone being blocked at this time??
	queue_extend(scheduler_mlq->A, wait_zone->A); // TODO: Add NULL checks
	queue_extend(scheduler_mlq->C, wait_zone->C);
	queue_extend(scheduler_mlq->T, wait_zone->T);
	free_queue(wait_zone->A);
	free_queue(wait_zone->C);
	free_queue(wait_zone->T);
	return;
}

// create the mlq for scheduler it self
MLQ_t* init_vehicle_scheduler(){
	return create_mlq();
}

// create the mlq for waiting zone it self
MLQ_t* init_vehicle_waiting_zone(){
	return create_mlq();
}

//check if a single v can be added to intersection
int check_fit(int intersection, Vehicle_t* v){
	//when there is no confict return 1
	return(!(v->intersection_segment_required & intersection));
}

// see if an intersection is full
int full(int intersection){return intersection == 7;} // TODO: Add explaination for TA, not intuitive without prior knowledge of how the intersection works

// remove the v from q and update intersection
void v_founded(Queue_t* q, int* intersection, Vehicle_t* v){
	//update value of intersection indicator
	*intersection |= v->intersection_segment_required;
	//remove v from q
	dequeue(v, q);
	//unf action that put the v into the intersection
	//update intersection state
	return;
}
// loop through a q and add all v that as long as it can fit in
int look_for_v_in_from_q(Queue_t* q, int* intersection){
	Vehicle_t* cur_v = q->head;
	//check head
	if(check_fit(intersection, cur_v)){
		v_founded(q, intersection, cur_v);
	}
	// quit when intersection is full
	if(full(*intersection)){return;}
	// loop through rest of the q
	while(cur_v != NULL){
	    if(check_fit(intersection, cur_v->next)){
			v_founded(q, intersection, cur_v);	
		}
		if(full(*intersection)){return;}
	}
	return;
}

// TODO: this function needs to be fixed. It is the entry point for the scheduler thread
void schedule_vehicles(MLQ_t* mlq, int* intersection){
	// TODO consume waiting queue, consume_waiting_zone

	// try to acquire locks for each intersection segment
	// see which locks can be acquired

	// find the next v
	// wake up the next v thread
	// sleep on an address, waiting for v thread to signal that it acquired the intersection locks
	// NOTE: Only the woken up vehicle thread and scheduler compete for intersection lock
	// 

	look_for_v_in_from_q(mlq->A, intersection); // find a v to put in intersection, then put into intersection
	if(full(*intersection)){return;}
	look_for_v_in_from_q(mlq->C, intersection);
	if(full(*intersection)){return;}
	look_for_v_in_from_q(mlq->T, intersection);
}

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
	//calculate intersection_segment_required
	v->intersection_segment_required = 2^(v->entrance);	// TODO: Explain how this works
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
{ 	// TODO: Explain how this works more clearly

	int exit;
	//calculate exit
	if(v->entrance == 0){exit = 2;}
	else{exit = v->entrance - 1;}
	//add the second critical section required
	v->intersection_segment_required = 7-2^(exit);  // TODO: Is this really needed at all?
}
//set the critical section of v
// TODO: A FUNCTION CALLED SET TURN SHOULD NOT THEN TURN THE VEHICLE! THIS IS CONFUSING! SEPERATION OF CONCERNS... why isn't this all just one function. 3 fucntion calls are being done to do one thing, which is to create the vehicle.
static void setturn(Vehicle_t* v){ // TODO: Is this really needed at all?
	if(v->turndirection == 0) { turnright(v); }
	else { turnleft(v); }
	return;
}

// TODO: locks need to be added to this function
// The vehicle enters the waiting zone
//approach adds a v into an mlq
static void approach(Vehicle_t *v, MLQ_t* mlq){
	if(v->vehicle_type == 0){ // TODO: Starvation may occur for all of these locks. Should this be fixed?
		// acquire lock
		lock_acquire(mlq->lockA);
		
		enqueue(v,mlq->A);
		
		// release lock
		lock_release(mlq->lockA);
	}
	if(v->vehicle_type == 1){
		// acquire lock
		lock_acquire(mlq->lockC);

		enqueue(v,mlq->C);

		// release lock
		lock_release(mlq->lockC);
	}
	if(v->vehicle_type == 2){
		// acquire lock
		lock_acquire(mlq->lockT);

		enqueue(v,mlq->T);
		
		// release lock
		lock_release(mlq->lockT);
	}
	else{print("Unknown Vehicle Type in approach(...)");}
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
static void approachintersection(MLQ_t* mlq, unsigned long vehiclenumber){
	Direction_t entrance;
	TurnDirection_t turndirection;
	VehicleType_t vehicletype;

	entrance = random() % 3;
	turndirection = random() % 2;
	vehicletype = random() % 3;

	// create vehicle and set turn
	Vehicle_t* v = create_vehicle(vehiclenumber, vehicletype, entrance, turndirection);

	// TODO: Is set turn even neede 
	setturn(v);
	// insert into waiting zone MLQ i.e. approached the intersection
	approach(v, mlq);

	// print state
	print_vehicle(v);
	
	// thread sleep
	// unf

	// .. thread moved into scheduler MLQ ... still sleeping

	// turn here after thread wake up occurs
	// acquire the locks for the intersection critical sections
	// relases the locks as it is exits each critical section
	// unf
}

// scheduler
void schedulerf(){
	// the critical section occupation indicator, range:[0,7]
	// each bit is 1 if occupied, else 0
	// bit 0 for AB
	// bit 1 for BC
	// bit 2 for CA
	// ex: AB and BC occupied, intersection_state = 3
	int* intersection_state = 0;
	MLQ_t* mlq = create_MLQ();
	while(/*unf if there is still v waiting*/1){
		//return if nobody is waiting
		if(/*unf if nobody is waiting*/1){return;}
		//unf cv lock
		schedule_vehicles(mlq, intersection_state);
		//unf unlock

	}
}
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

/*
	Effectively the main function for the program.
	Initializes global variables. Creates the vehicle threads.

*/
int createvehicles(int nargs, char ** args){
	int index, error;
	MLQ_t* mlq = mlq_create();
	/*
	 * Avoid unused variable warnings.
	 */
	(void) nargs;
	(void) args;
	
	// initialize global variables: scheduler, waiting_zone, iseg locks
	const char *isegAB_name = "isegAB";
	const char *isegBC_name = "isegBC";
	const char *isegCA_name = "isegCA";
	isegAB_lock = lock_create(isegAB_name);
	isegBC_lock = lock_create(isegBC_name);
	isegCA_lock = lock_create(isegCA_name);

	numExitedV = 0;
	const char *inumExitedVLock_name = "isegCA";
	numExitedVLock = lock_create(inumExitedVLock_name);

	vehicle_scheduler = mlq_init();
	waiting_zone = mlq_init();

	// TODO: Set up the scheduler thread, scheduler thread remains until NVEHICLES have exited the intersection

	// create the vehicle scheduler thread
	error = thread_fork("scheduler thread", NULL, index, schedule_vehicles,NULL);
	if (error) {
		panic("scheduler: thread_fork failed: %s\n",strerror(error)); 
	}
		
	/*
	 * Start NVEHICLES approachintersection() threads.
	 */
	for (index = 0; index < NVEHICLES; index++) {

		error = thread_fork("approachintersection thread", NULL, index, approachintersection, NULL);

		/*
		 * panic() on error.
		 */
		if (error) {
			panic("approachintersection: thread_fork failed: %s\n",
					strerror(error)
				 );
		}
	}


	// TODO: thread join

	return 0;
}
