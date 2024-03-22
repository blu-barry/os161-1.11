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
// #include <stdio.h> // why can this be found during compilation?
// #include <stdlib.h>

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
	AMBULANCE = 0,
	CAR = 1,
	TRUCK = 2
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

Vehicle_t* Vehicle_create(int vehiclenumber, VehicleType_t vehicle_type, Direction_t entrance, TurnDirection_t turndirection);
const char* createVehicleLockNameString(unsigned long lockNumber);
int Vehicle_free(Vehicle_t* vehicle);

Queue_t* Queue_init();
int Queue_isEmpty(Queue_t *q);
int Queue_enqueue(Queue_t *q, Vehicle_t *vehicle);
Vehicle_t* Queue_dequeue(Queue_t *q);
int Queue_free(Queue_t *q);

int Queue_produce(Queue_t *q, Vehicle_t *vehicle);
int Queue_consume(Queue_t *pq, Queue_t *dq);
int Waiting_zone_produce(Vehicle_t *v);

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
Vehicle_t* Vehicle_create(int vehiclenumber, VehicleType_t vehicle_type, Direction_t entrance, TurnDirection_t turndirection) {
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
		Vehicle_free(v);
		return NULL;
	}
	v->lock = lock_create(lockName);
	v->next = NULL;
	return v;
}

const char* createVehicleLockNameString(unsigned long lockNumber) {
	// Prefix string
    const char* prefix = "Vehicle Lock Number: ";
    // Calculate the total length needed for the string (+1 for null terminator)
    int totalLength = snprintf(NULL, 0, "%s%lu", prefix, lockNumber) + 1;
    
    // Dynamically allocate memory for the full string
    char* fullString = (char*)kmalloc(totalLength);
    if (fullString == NULL) {
        // Memory allocation failed
        return NULL;
    }

    // Construct the full string
    int written = snprintf(fullString, totalLength, "%s%lu", prefix, lockNumber);
    if (written < 0 || written >= totalLength) {
        // snprintf failed or buffer size was underestimated, handle error
        kfree(fullString);
        return NULL;
    }
    
    // Return the dynamically allocated full string
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

//MLQ
MLQ_t* mlq_init(){
	MLQ_t* mlq = (MLQ_t*)kmalloc(sizeof(MLQ_t));
	if(mlq==NULL){ return NULL; }

	mlq->A = Queue_init();
	if (mlq->A == NULL) {
		kfree(mlq);
		return NULL;
	}

	mlq->C = Queue_init();
	if (mlq->C == NULL) {
		Queue_free(mlq->A);
		kfree(mlq);
		return NULL;
	}

	mlq->T = Queue_init();
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
	Queue_free(mlq->A);
	Queue_free(mlq->C);
	Queue_free(mlq->T); // TODO: Does Queue_free and lock_destroy do NULL checks?
	lock_destroy(mlq->lockA);
	lock_destroy(mlq->lockC);
	lock_destroy(mlq->lockT);
	kfree(mlq);
	return;
}

/* 	Waiting Zone Functions 
	NOTE: These functions treat the queue effectively like a linked list.
*/

/*	The vehicle is inserted into the proper queue in the waiting zone MLQ.
	This is used in approachintersection(...). 
*/
int Waiting_zone_produce(Vehicle_t *v) {
	if (v == NULL) {
		return ERROR_NULL_POINTER;
	}
	if (v->vehicle_type == AMBULANCE) {
		Queue_produce(waiting_zone->A, v);
		return SUCCESS;
	} else if (v->vehicle_type == CAR) {
		Queue_produce(waiting_zone->C, v);
		return SUCCESS;
	} else if (v->vehicle_type == TRUCK) {
		Queue_produce(waiting_zone->T, v);
		return SUCCESS;
	} else {
		return ERROR_INVALID_OPERATION;
	}
}

/*	Used by the scheduler to consume the waiting zone vehicle queues.

*/
int Waiting_zone_consume() {
	if (Queue_consume(waiting_zone->A, vehicle_scheduler->A) != SUCCESS) {
		// TODO: Debug message
	} else if (Queue_consume(waiting_zone->C, vehicle_scheduler->C) != SUCCESS) {
		// TODO: Debug message
	} else if (Queue_consume(waiting_zone->T, vehicle_scheduler->T) != SUCCESS) {
		// TODO: Debug message
	}
}

/* 	Uses hand over hand locking to enqueue a Vehicle_t node to the Queue_t.
	This is used by the waiting zone produce function to add new Vehicle_t nodes.
*/
int Queue_produce(Queue_t *q, Vehicle_t *vehicle) {
    if (q == NULL || vehicle == NULL) {
        return ERROR_NULL_POINTER; // Indicate failure
    }

    vehicle->next = NULL;

    lock_acquire(q->tail->lock); // Lock the current tail node

    q->tail->next = vehicle; // Add the new node to the queue
    q->tail = vehicle; // Update the tail pointer to the new node

    lock_release(q->tail->lock); // Release the lock on the new tail node, which is the vehicle itself

    q->size++;
    return q->size; // Return new size of the queue as indication of success
}

/*	Uses hand over hand locking to iterate over the Queue_t *pq and remove vehicle nodes as it goes then adding the nodes to the specified Queue_t *dq.

*/
int Queue_consume(Queue_t *pq, Queue_t *dq) {
    if (pq == NULL || pq->size == 0 || dq == NULL) {
        return ERROR_NULL_POINTER;
    }

	lock_acquire(pq->head->lock); // Lock the dummy head node first
    Vehicle_t* current = pq->head->next; // Start with the first real node

	while (current != NULL) {
        lock_acquire(current->lock); // Lock the current node

        // pop the current node by adjusting pointers
        pq->head->next = current->next;
        if (pq->tail == current) {
            pq->tail = pq->head; // Update tail if we're removing the last node
        }

		// Prepare for the next iteration
        Vehicle_t* next = current->next;
		
		current->next = NULL;
		lock_release(current->lock);

		// move the node from the producing queue pq to the destination queue dq
		Queue_produce(dq, current);
        
        current = next; // Advance to the next node
    }

	lock_release(pq->head->lock); // Release the dummy head node's lock

    // Reset the queue
    pq->size = 0;
	return SUCCESS;
}

Vehicle_t* Scheduler_search_for_next_serviceable_vehicle(Queue_t *q) {
	if (q == NULL) {
		return NULL;
	}

	// iterate over the queue and check each if each vehicle can be serviced. If the vehicle can be serviced, then wake up the vehicle and allow it to cross the intersection
	lock_acquire(q->head->lock); // Lock the dummy head node first
    Vehicle_t* current = q->head->next; // Start with the first real node

	while (current != NULL) {
		// TODO: should I try to acquire all of the iseg locks again?
        lock_acquire(current->lock); // Lock the current node

		// lock_try_acquire_alert(isegAB_lock); // THIS MAY RESULT IN TRUCKS GOING BEFORE CARS THOUGH AN APPROACH LIKE THIS MAY BE MORE EFFICIENT
		// lock_try_acquire_alert(isegBC_lock);
		// lock_try_acquire_alert(isegCA_lock);

		// Prepare for the next iteration
        Vehicle_t* next;

		// This is not the most pretty implementation but it is straightforward and works
		// Is the car serviceable?
		if (current->entrance == A) {
			if (current->turndirection == L) { // need AB and BC
				if (lock_do_i_hold(isegAB_lock) && lock_do_i_hold(isegBC_lock)) {
					// service the car

					// pop node from list, release iseg locks, wake up vehicle thread, then sleep on sleepAddr, wait for vehicle thread to acquire iseg locks, then continue
					
					// pop the current node by adjusting pointers
					q->head->next = current->next;
					if (q->tail == current) {
						q->tail = q->head; // Update tail if we're removing the last node
					}

					// Prepare for the next iteration
					next = current->next;
					
					current->next = NULL;
					lock_release(current->lock); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegAB_lock);
					lock_release(isegBC_lock);
					thread_wakeup(current->vehiclenumber);
					thread_sleep(vehicle_scheduler->sleepAddr); // wait for the vehicle thread to wake scheduler up after acquiring the iseg locks
					
					current = next; // Advance to the next node
				} else { // car can not be serviced
					current = current->next; // Advance to the next node
				} 
			} else if (current->turndirection == R) { // need AB
				if (lock_do_i_hold(isegAB_lock)) {
					// service the car

					// pop node from list, release iseg locks, wake up vehicle thread, then sleep on sleepAddr, wait for vehicle thread to acquire iseg locks, then continue
					// pop the current node by adjusting pointers
					q->head->next = current->next;
					if (q->tail == current) {
						q->tail = q->head; // Update tail if we're removing the last node
					}

					// Prepare for the next iteration
					next = current->next;
					
					current->next = NULL;
					lock_release(current->lock); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegAB_lock);
					thread_wakeup(current->vehiclenumber);
					thread_sleep(vehicle_scheduler->sleepAddr); // wait for the vehicle thread to wake scheduler up after acquiring the iseg locks
					
					current = next; // Advance to the next node
				} else { // car can not be serviced
					current = current->next; // Advance to the next node
				}
			}
		} else if (current->entrance == B) {
			if (current->turndirection == L) { // need BC and CA
				if (lock_do_i_hold(isegBC_lock) && lock_do_i_hold(isegCA_lock)) {
					// service the car

					// pop node from list, release iseg locks, wake up vehicle thread, then sleep on sleepAddr, wait for vehicle thread to acquire iseg locks, then continue
					// pop the current node by adjusting pointers
					q->head->next = current->next;
					if (q->tail == current) {
						q->tail = q->head; // Update tail if we're removing the last node
					}

					// Prepare for the next iteration
					next = current->next;
					
					current->next = NULL;
					lock_release(current->lock); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegBC_lock);
					lock_release(isegCA_lock);
					thread_wakeup(current->vehiclenumber);
					thread_sleep(vehicle_scheduler->sleepAddr); // wait for the vehicle thread to wake scheduler up after acquiring the iseg locks
					
					current = next; // Advance to the next node
				} else { // car can not be serviced
					current = current->next; // Advance to the next node
				} 
			} else if (current->turndirection == R) { // need BC
				if (lock_do_i_hold(isegBC_lock)) {
					// service the car

					// pop node from list, release iseg locks, wake up vehicle thread, then sleep on sleepAddr, wait for vehicle thread to acquire iseg locks, then continue
					// pop the current node by adjusting pointers
					q->head->next = current->next;
					if (q->tail == current) {
						q->tail = q->head; // Update tail if we're removing the last node
					}

					// Prepare for the next iteration
					next = current->next;
					
					current->next = NULL;
					lock_release(&(current->lock)); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegBC_lock);
					thread_wakeup(current->vehiclenumber);
					thread_sleep(vehicle_scheduler->sleepAddr); // wait for the vehicle thread to wake scheduler up after acquiring the iseg locks
					
					current = next; // Advance to the next node
				} else { // car can not be serviced
					current = current->next; // Advance to the next node
				} 
			}
		} else if (current->entrance == C) {
			if (current->turndirection == L) { // need CA and AB
				if (lock_do_i_hold(isegCA_lock) && lock_do_i_hold(isegAB_lock)) {
					// service the car

					// pop node from list, release iseg locks, wake up vehicle thread, then sleep on sleepAddr, wait for vehicle thread to acquire iseg locks, then continue
					// pop the current node by adjusting pointers
					q->head->next = current->next;
					if (q->tail == current) {
						q->tail = q->head; // Update tail if we're removing the last node
					}

					// Prepare for the next iteration
					next = current->next;
					
					current->next = NULL;
					lock_release(&(current->lock)); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegCA_lock);
					lock_release(isegAB_lock);
					thread_wakeup(current->vehiclenumber);
					thread_sleep(vehicle_scheduler->sleepAddr); // wait for the vehicle thread to wake scheduler up after acquiring the iseg locks
					
					current = next; // Advance to the next node
				} else { // car can not be serviced
					current = current->next; // Advance to the next node
				} 
			} else if (current->turndirection == R) { // need CA
				if (lock_do_i_hold(isegCA_lock)) {
					// service the car

					// pop node from list, release iseg locks, wake up vehicle thread, then sleep on sleepAddr, wait for vehicle thread to acquire iseg locks, then continue
					// pop the current node by adjusting pointers
					q->head->next = current->next;
					if (q->tail == current) {
						q->tail = q->head; // Update tail if we're removing the last node
					}

					// Prepare for the next iteration
					next = current->next;
					
					current->next = NULL;
					lock_release(&(current->lock)); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegCA_lock);
					thread_wakeup(current->vehiclenumber);
					thread_sleep(vehicle_scheduler->sleepAddr); // wait for the vehicle thread to wake scheduler up after acquiring the iseg locks
					
					current = next; // Advance to the next node
				
				} else { // car can not be serviced
					current = current->next; // Advance to the next node
				} 
			}
		} else {
			// TODO: ERROR THROWN HERE
			// car can not be serviced
			current = current->next; // Advance to the next node
		}
    }

	lock_release(&(q->head->lock)); // Release the dummy head node's lock


}

/* The main function for the scheduler thread. It schedules vehicles as they approach the intersection. Continues to look until the number of exited cars equals NVEHICLES.

*/
int Schedule_vehicles() {

	// I am using a go to here in order to reaquire the lock before checking the condition, after releaseing it during the previous iteration. Though it is often not recommended to use GOTOs, in this situation it seems applicable.
schedule_iteration:
	lock_acquire(numExitedVLock);
	while (numExitedV < NVEHICLES) { // schedule more vehicles
		lock_release(numExitedVLock);
		// consume the waiting zone
		Waiting_zone_consume();

		// see what locks can be acquired
		lock_try_acquire_alert(isegAB_lock);
		lock_try_acquire_alert(isegBC_lock);
		lock_try_acquire_alert(isegCA_lock);

		// TODO: how to identify what vehicles should be woken up based on what locks I have? lock_do_i_own?
		// service ambulances first
		lock_acquire(vehicle_scheduler->lockA);
		Scheduler_search_for_next_serviceable_vehicle(vehicle_scheduler->A);
		lock_release(vehicle_scheduler->lockA);

		// TODO: Function to search queue for car that needs available locks
		// TODO: Function to remove car from queue
		// TODO: Function to wake up car

		//TODO: add function to send carss


		// service cars next
		lock_acquire(vehicle_scheduler->lockC);
		Scheduler_search_for_next_serviceable_vehicle(vehicle_scheduler->C);
		lock_release(vehicle_scheduler->lockC);
		// service trucks last
		lock_acquire(vehicle_scheduler->lockT);
		Scheduler_search_for_next_serviceable_vehicle(vehicle_scheduler->T);
		lock_release(vehicle_scheduler->lockT);

		goto schedule_iteration;
	}




}

// scheduler functions

// TODO: implement hand of hand locking when consuming the waiting zone
// need to acquire all of the locks for a turn
//absorb v from wait zone and leave an empty body
void consume_waiting_zone(MLQ_t* wait_zone, MLQ_t* scheduler_mlq){ // TODO: How is the waiting zone being blocked at this time??
	queue_extend(scheduler_mlq->A, wait_zone->A); // TODO: Add NULL checks
	queue_extend(scheduler_mlq->C, wait_zone->C);
	queue_extend(scheduler_mlq->T, wait_zone->T);
	Queue_free(wait_zone->A);
	Queue_free(wait_zone->C);
	Queue_free(wait_zone->T);
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
	// acquire iseg_lock then wake up scheduler effectively notifying it that it entered the intersection 
	// print when vehicle enters and exits
	if (v->entrance == A) {			// acquire isegAB_lock

	} else if (v->entrance == B) {	// acquire isegBC_lock
		
	} else if (v->entrance == C) {	// acquire isegCA_lock

	}
	
	//calculate intersection_segment_required
	// v->intersection_segment_required = 2^(v->entrance);	// TODO: Explain how this works
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

	// create vehicle 
	Vehicle_t* v = Vehicle_create(vehiclenumber, vehicletype, entrance, turndirection);

	// insert into waiting zone
	Waiting_zone_produce(v);

	// thread sleep. wait to be woken up by scheduler
	thread_sleep(&v->vehiclenumber);
	
	// execute turn
	// acquire the intersection locks
	lock_acquire(v->lock);
	if (v->entrance == A) {
		if (v->turndirection == L) { // acquire AB and BC
			lock_acquire(isegAB_lock);
			lock_acquire(isegBC_lock);
			thread_wakeup(vehicle_scheduler->sleepAddr); // wake up scheduler so that it can resume
			kprintf("Entered Intersection Turning Left: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// TODO: print to cross intersection
			kprintf("Entered Intersection Segment AB: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// release locks
			lock_release(isegAB_lock);
			kprintf("Exited Intersection Segment AB: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			kprintf("Entered Intersection Segment BC: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			lock_release(isegBC_lock);
			kprintf("Exited Intersection Segment BC: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
		} else if (v->turndirection == R) { // acquire AB
			lock_acquire(isegAB_lock);
			thread_wakeup(vehicle_scheduler->sleepAddr); // wake up scheduler so that it can resume
			kprintf("Entered Intersection Turning Right: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// TODO: print to cross intersection
			kprintf("Entered Intersection Segment AB: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// release locks
			lock_release(isegAB_lock);
			kprintf("Exited Intersection Segment AB: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
		}
	} else if (v->entrance == B) {
		if (v->turndirection == L) { // acquire BC and CA
			lock_acquire(isegBC_lock);
			lock_acquire(isegCA_lock);
			thread_wakeup(vehicle_scheduler->sleepAddr); // wake up scheduler so that it can resume
			kprintf("Entered Intersection Turning Left: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// TODO: print to cross intersection
			kprintf("Entered Intersection Segment BC: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// release locks
			lock_release(isegBC_lock);
			kprintf("Exited Intersection Segment BC: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			kprintf("Entered Intersection Segment CA: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			lock_release(isegCA_lock);
			kprintf("Exited Intersection Segment CA: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
		} else if (v->turndirection == R) { // acquire BC
			lock_acquire(isegBC_lock);
			thread_wakeup(vehicle_scheduler->sleepAddr); // wake up scheduler so that it can resume
			kprintf("Entered Intersection Turning Right: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// TODO: print to cross intersection
			kprintf("Entered Intersection Segment BC: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// release locks
			lock_release(isegBC_lock);
			kprintf("Exited Intersection Segment BC: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
		}
	} else if (v->entrance == C) {
		if (v->turndirection == L) { // acquire CA and AB
			lock_acquire(isegCA_lock);
			lock_acquire(isegAB_lock);
			thread_wakeup(vehicle_scheduler->sleepAddr); // wake up scheduler so that it can resume
			kprintf("Entered Intersection Turning Left: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// TODO: print to cross intersection
			kprintf("Entered Intersection Segment CA: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// release locks
			lock_release(isegCA_lock);
			kprintf("Exited Intersection Segment CA: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			kprintf("Entered Intersection Segment AB: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			lock_release(isegAB_lock);
			kprintf("Exited Intersection Segment AB: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			
		} else if (v->turndirection == R) { // acquire CA
			lock_acquire(isegCA_lock);
			thread_wakeup(vehicle_scheduler->sleepAddr); // wake up scheduler so that it can resume
			kprintf("Entered Intersection Turning Right: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// TODO: print to cross intersection
			kprintf("Entered Intersection Segment CA: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
			// release locks
			lock_release(isegCA_lock);
			kprintf("Exited Intersection Segment CA: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);

		}
	}
	// kprintf("%s\n", str);
	// kkprintf("%lu\n", v->vehiclenumber);

	// print vehicle, exited vehicle
	DEBUG(DB_THREADS, "Exited Intersection Completely: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
	kprintf("Exited Intersection Completely: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
	pprintf("Exited Intersection Completely: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
	// release vehicle lock
	lock_release(v->lock);
	Vehicle_free(v->lock);
	// free vehicle struct
	// TODO: increment num exited v counter
	lock_acquire(numExitedVLock);
	numExitedV++;
	lock_release(numExitedVLock);
	// TODO: Is set turn even neede 
	// setturn(v);
	// insert into waiting zone MLQ i.e. approached the intersection
	// approach(v, mlq);

	// print state
	// print_vehicle(v);
	
	// thread sleep
	// unf

	// .. thread moved into scheduler MLQ ... still sleeping

	// turn here after thread wake up occurs
	// acquire the locks for the intersection critical sections
	// relases the locks as it is exits each critical section
	// unf
}

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
	printf("createvehicles started printf"); // neither of these print?? how to properly print?
	kprintf("createvehicles started kprintf");
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
	error = thread_fork("scheduler thread", NULL, index, Schedule_vehicles,NULL);
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
