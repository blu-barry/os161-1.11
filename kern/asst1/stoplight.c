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
    ERROR_LOCK_CREATION_FAILED = -5,
    ERROR_QUEUE_FULL = -6,              // The queue is full (for fixed-size queues)
    ERROR_QUEUE_EMPTY = -7,             // The queue is empty, nothing to consume
	ERROR_QUEUE_CONSUME_FAILED = -8,
    ERROR_QUEUE_NOT_INITIALIZED = -9,
    ERROR_MEMORY_ALLOCATION_FAILED = -10,// Memory allocation failed
    ERROR_INVALID_OPERATION = -11        // Invalid operation attempted
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
	unsigned long  vehiclenumber;						// Unsigned long since it is based on NVEHICLES. 
	VehicleType_t vehicle_type;
	Direction_t entrance;
	TurnDirection_t turndirection;
	lock_t* lock;										// used in the hand over hand locking mechanism
    lock_t* sleepAddr;
	struct Vehicle* next;	
	// int* sleepAddr;										// The address the vehicle thread sleeps on when it enters the waiting_zone. Eventually the scheduler will wake it up via this address.
} Vehicle_t;

/*	A basic queue implementation
	This implementation of a queue does not use dummy head or tail nodes.
	New nodes are added to the tail of the queue. Dequeue removes the node that head points to.
*/
typedef struct Queue {
	Vehicle_t* head;    // Points to dummy head
	Vehicle_t* tail;    // Points to the last real node or dummy if empty
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
	// int* sleepAddr;		// a generic value that is used in the scheduler to sleep on, ultimately allowing other threads to wake it up.
    lock_t* sleepAddr;
} MLQ_t;

/* Function Prototypes */

Vehicle_t* Vehicle_create(int vehiclenumber, VehicleType_t vehicle_type, Direction_t entrance, TurnDirection_t turndirection);
const char* createVehicleLockNameString(unsigned long lockNumber);
const char* formatVehicleMessage(const Vehicle_t* v, const char* messagePrefix);
int Vehicle_free(Vehicle_t* vehicle);

Queue_t* Queue_init();
int Queue_enqueue(Queue_t *q, Vehicle_t *vehicle);
Vehicle_t* Queue_dequeue(Queue_t *q);
int Queue_free(Queue_t *q);

int Queue_produce(Queue_t *q, Vehicle_t *vehicle);
int Queue_consume(Queue_t *pq, Queue_t *dq);
int Waiting_zone_produce(Vehicle_t *v);

MLQ_t* mlq_init();
int mlq_free(MLQ_t* mlq);

// TODO: add the remaining function prototypes
static void  Schedule_vehicles();
int Scheduler_search_for_next_serviceable_vehicle(Queue_t *q);
int service_vehicle_from_entrance_A(Vehicle_t *v);
int service_vehicle_from_entrance_B(Vehicle_t *v);
int service_vehicle_from_entrance_C(Vehicle_t *v);
void remove_vehicle_from_queue(Queue_t *q, Vehicle_t *v);
void allow_vehicle_to_cross_intersection(Vehicle_t *v);

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

/**
 * Creates a new Vehicle instance with specified attributes.
 * 
 * @param vehiclenumber The unique identifier for the vehicle.
 * @param vehicle_type The type of the vehicle (e.g., CAR, TRUCK, AMBULANCE).
 * @param entrance The entrance direction from which the vehicle approaches the intersection.
 * @param turndirection The turning direction of the vehicle at the intersection.
 * @return A pointer to the newly created Vehicle instance, or NULL if the creation fails.
 */
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
    if (v->lock == NULL) {
        DEBUG(DB_THREADS, "Lock creation failed in Vehicle_create\n");
        Vehicle_free(v);
        return NULL;
    }

	v->next = NULL;
	// Allocate memory for the integer pointer
    // v->sleepAddr = kmalloc(sizeof(int));
	// if (v->sleepAddr == NULL) {
	// 	Vehicle_free(v);
	// 	return NULL;
	// }
	// *(v->sleepAddr) = 0;
    v->sleepAddr = lock_create("sleepaddr");
    if (v->sleepAddr == NULL) {
        DEBUG(DB_THREADS, "sleepAddr creation failed in Vehicle_create\n");
        Vehicle_free(v);
        return NULL;
    }

	return v;
}

/**
 * Generates a unique lock name string for a vehicle based on its number.
 * 
 * @param lockNumber The vehicle's unique number to be included in the lock name.
 * @return A dynamically allocated string containing the lock name, or NULL if memory allocation fails.
 */
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

/**
 * Formats a message string describing a vehicle's current state.
 * 
 * @param v A pointer to the vehicle instance.
 * @param messagePrefix A prefix string to prepend to the message.
 * @return A statically allocated string containing the formatted message. Note: Subsequent calls to this function will overwrite the previous result.
 */
const char* formatVehicleMessage(const Vehicle_t* v, const char* messagePrefix) {
    static char buffer[256];
    int requiredSize;

    // Calculate the required size (+1 for null terminator)
    requiredSize = snprintf(NULL, 0, "%s: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }",
                            messagePrefix, v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection) + 1;

    if (requiredSize > sizeof(buffer)) { //TODO: warning: comparison between signed and unsigned integer expressions
        // Handle error: message size exceeds buffer capacity
        // For this example, let's indicate an error in the buffer
        snprintf(buffer, sizeof(buffer), "Error: message exceeds buffer size.");
    } else {
        // Safely format the message into the buffer
        snprintf(buffer, sizeof(buffer), "%s: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }",
                 messagePrefix, v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
    }

    return buffer;
}

/**
 * Frees the resources associated with a vehicle instance.
 * 
 * @param vehicle A pointer to the vehicle instance to be freed.
 * @return SUCCESS if the operation was successful, ERROR_NULL_POINTER if the vehicle pointer is NULL.
 */
int Vehicle_free(Vehicle_t* vehicle) {
	if (vehicle == NULL) {
        return ERROR_NULL_POINTER;
    }

	if (vehicle->lock != NULL) {
        lock_destroy(vehicle->lock); // Assume success
        vehicle->lock = NULL;
    }
	if (vehicle->sleepAddr != NULL) {
		// kfree(vehicle->sleepAddr);
		// vehicle->sleepAddr = NULL;
        lock_destroy(vehicle->sleepAddr);
	}

	kfree(vehicle);

    return SUCCESS; // Indicate success
}

/* Queue Functions */

/**
 * Initializes a new queue with a dummy head node.
 * 
 * @return A pointer to the newly created Queue instance, or NULL if the creation fails.
 */
Queue_t* Queue_init() {
    Queue_t* q = (Queue_t*)kmalloc(sizeof(Queue_t));
    if (!q) {
        return NULL;
    }

    // Initialize a dummy node
    Vehicle_t* dummy = (Vehicle_t*)kmalloc(sizeof(Vehicle_t));
    if (!dummy) {
        kfree(q);
        return NULL;
    }
    dummy->next = NULL;
    dummy->lock = lock_create("dummy lock");
    if (!dummy->lock) {
        kfree(dummy);
        kfree(q);
        return NULL;
    }

    q->head = q->tail = dummy;
    q->size = 0;

    return q;
}

/**
 * Adds a new vehicle to the end of the queue.
 * 
 * @param q A pointer to the queue where the vehicle will be added.
 * @param vehicle A pointer to the vehicle to be added to the queue.
 * @return SUCCESS on success, an error code on failure (e.g., ERROR_NULL_POINTER, ERROR_LOCK_CREATION_FAILED).
 */
int Queue_enqueue(Queue_t *q, Vehicle_t *vehicle) {
    if (!q || !vehicle) {
        return ERROR_NULL_POINTER;
    }

    vehicle->next = NULL;
    vehicle->lock = lock_create("vehicle lock");
    if (!vehicle->lock) {
        // Handle lock creation failure
        Vehicle_free(vehicle);
        return ERROR_LOCK_CREATION_FAILED;
    }

    lock_acquire(q->tail->lock);
    q->tail->next = vehicle;
    q->tail = vehicle;
    q->size++;
    lock_release(q->tail->lock); // Previously acquired on the old tail

    return SUCCESS; // Assuming SUCCESS is defined as 0
}


/**
 * Removes and returns the first vehicle from the queue.
 * 
 * @param q A pointer to the queue from which the vehicle will be removed.
 * @return A pointer to the dequeued vehicle, or NULL if the queue is empty or on error.
 */
Vehicle_t* Queue_dequeue(Queue_t *q) {
    if (!q || q->size == 0) {
        return NULL;
    }

    lock_acquire(q->head->lock);
    Vehicle_t* temp = q->head; // Temporary pointer to the dummy head
    Vehicle_t* dequeuedVehicle = temp->next; // The first real node to dequeue

    if (!dequeuedVehicle) {
        lock_release(temp->lock);
        return NULL; // Queue was empty besides dummy node
    }

    lock_acquire(dequeuedVehicle->lock);
    q->head = dequeuedVehicle; // Move head to point to the next real node
    temp->next = dequeuedVehicle->next; // Dummy node points to the next node
    if (q->tail == dequeuedVehicle) { // If we are removing the last real node
        q->tail = q->head; // Queue is empty, tail reverts to dummy
    }
    q->size--;
    lock_release(temp->lock);
    lock_release(dequeuedVehicle->lock); // We can now release the dequeued node's lock

    dequeuedVehicle->next = NULL; // Just to clean up

    return dequeuedVehicle;
}

/**
 * Frees the resources associated with a queue and all its contained vehicles.
 * 
 * @param q A pointer to the queue to be freed.
 * @return SUCCESS if the operation was successful, ERROR_NULL_POINTER if the queue pointer is NULL.
 */
int Queue_free(Queue_t *q) {
    if (!q) {
        return ERROR_NULL_POINTER;
    }

    Vehicle_t* current = q->head;
    while (current != NULL) {
        Vehicle_t* temp = current;
        current = current->next;
        if (temp->lock) {
            lock_destroy(temp->lock);
        }
        Vehicle_free(temp);
    }

    kfree(q);

    return SUCCESS;
}

/**
 * Initializes a new Multilevel Queue (MLQ) for managing vehicles of different types.
 * 
 * @return A pointer to the newly created MLQ instance, or NULL if the creation fails.
 */
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
    assert(mlq->lockA != NULL);
    assert(mlq->lockC != NULL);
    assert(mlq->lockT != NULL);
	
	// Allocate memory for the integer pointer
    // mlq->sleepAddr = kmalloc(sizeof(int));
	// if (mlq->sleepAddr == NULL) {
	// 	kfree(mlq->sleepAddr);
	// 	mlq_free(mlq);
	// 	return NULL;
	// }
	// *(mlq->sleepAddr) = 0;
    mlq->sleepAddr = lock_create("sleepaddr");
    if (mlq->sleepAddr == NULL) {
        DEBUG(DB_THREADS, "sleepAddr creation failed in Vehicle_create\n");
        mlq_free(mlq);
        return NULL;
    }
	DEBUG(DB_THREADS, "MLQ initialized\n");
	return mlq;
}

/**
 * Frees the resources associated with a Multilevel Queue (MLQ) and all its contained queues and vehicles.
 * 
 * @param mlq A pointer to the MLQ instance to be freed.
 * @return SUCCESS if the operation was successful, ERROR_NULL_POINTER if the MLQ pointer is NULL.
 */
int mlq_free(MLQ_t* mlq) {
	if (mlq == NULL) {
		return ERROR_NULL_POINTER;
	}

	if (Queue_free(mlq->A)) {
		// TODO; Debug error?
	}
	lock_destroy(mlq->lockA);

	if (Queue_free(mlq->C)) {
		// TODO; Debug error?
	}
	lock_destroy(mlq->lockC);

	if (Queue_free(mlq->T)) {
		// TODO; Debug error?
	}
	lock_destroy(mlq->lockT);
	// if (mlq->sleepAddr != NULL) {
	// 	kfree(mlq->sleepAddr);
	// 	mlq->sleepAddr = NULL;
	// }
    if (mlq->sleepAddr != NULL) {
		// kfree(vehicle->sleepAddr);
		// vehicle->sleepAddr = NULL;
        lock_destroy(mlq->sleepAddr);
	}

	DEBUG(DB_THREADS, "MLQ freed\n");
	return SUCCESS;
}

/**
 * Moves vehicles from the waiting zone to the scheduler's queues for processing.
 * 
 * @return SUCCESS if all vehicles were successfully moved, otherwise ERROR_QUEUE_CONSUME_FAILED if any queue operation fails.
 */
int Waiting_zone_consume() {
    int result = SUCCESS;

    // Acquire locks for all destination queues
    lock_acquire(vehicle_scheduler->lockA);
    lock_acquire(vehicle_scheduler->lockC);
    lock_acquire(vehicle_scheduler->lockT);

    // Check if the source queues are empty before consuming
    if (waiting_zone->A->size > 0) {
        if (Queue_consume(waiting_zone->A, vehicle_scheduler->A) != SUCCESS) {
            result = ERROR_QUEUE_CONSUME_FAILED;
            // TODO: Debug message
			DEBUG(DB_THREADS, "Queue consume ambulance failed\n");
        }
    }

    if (waiting_zone->C->size > 0) {
        if (Queue_consume(waiting_zone->C, vehicle_scheduler->C) != SUCCESS) {
            result = ERROR_QUEUE_CONSUME_FAILED;
            // TODO: Debug message
			DEBUG(DB_THREADS, "Queue consume car failed\n");
        }
    }

    if (waiting_zone->T->size > 0) {
        if (Queue_consume(waiting_zone->T, vehicle_scheduler->T) != SUCCESS) {
            result = ERROR_QUEUE_CONSUME_FAILED;
            // TODO: Debug message
			DEBUG(DB_THREADS, "Queue consume truck failed\n");
        }
    }

    // Release locks for all destination queues
    lock_release(vehicle_scheduler->lockT);
    lock_release(vehicle_scheduler->lockC);
    lock_release(vehicle_scheduler->lockA);

    return result;
}

/**
 * Inserts a vehicle into the appropriate queue in the waiting zone based on its type.
 * 
 * @param v A pointer to the vehicle to be inserted.
 * @return SUCCESS if the operation was successful, otherwise an error code (e.g., ERROR_NULL_POINTER, ERROR_INVALID_OPERATION).
 */
int Queue_produce(Queue_t *q, Vehicle_t *vehicle) {
    if (q == NULL || vehicle == NULL) {
        return ERROR_NULL_POINTER; // Indicate failure
    }

    vehicle->next = NULL;

    // TODO: I think the dummy nodes may be the issue
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
    if (pq == NULL || dq == NULL) {
        return ERROR_NULL_POINTER;
    }
    if (pq->size == 0) {
        return ERROR_QUEUE_EMPTY;
    }

    lock_acquire(pq->head->lock); // Lock the dummy head node of the producing queue first
    lock_acquire(dq->head->lock); // Lock the dummy head node of the destination queue

    Vehicle_t* current = pq->head->next; // Start with the first real node

    while (current != NULL) {
        lock_acquire(current->lock); // Lock the current node

        // Pop the current node by adjusting pointers
        pq->head->next = current->next;
        if (pq->tail == current) {
            pq->tail = pq->head; // Update tail if we're removing the last node
        }

        // Prepare for the next iteration
        Vehicle_t* next = current->next;
        current->next = NULL;
        lock_release(current->lock);

        // Move the node from the producing queue pq to the destination queue dq
        Queue_produce(dq, current);

        current = next; // Advance to the next node
    }

    lock_release(dq->head->lock); // Release the dummy head node's lock of the destination queue

	// Reset the producing queue
    pq->size = 0;
    lock_release(pq->head->lock); // Release the dummy head node's lock of the producing queue
    return SUCCESS;
}

/* 	Iterates over each of the scheduler queues and tries to allow cars to cross the intersection depending on their priority and the locks available


*/
int Scheduler_search_for_next_serviceable_vehicle(Queue_t *q) {
	if (q == NULL) {
		return ERROR_NULL_POINTER;
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
					thread_wakeup(current->sleepAddr);
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
					thread_wakeup(current->sleepAddr);
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
					thread_wakeup(current->sleepAddr);
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
					lock_release(current->lock); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegBC_lock);
					thread_wakeup(current->sleepAddr);
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
					lock_release(current->lock); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegCA_lock);
					lock_release(isegAB_lock);
					thread_wakeup(current->sleepAddr);
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
					lock_release(current->lock); // TODO: Not sure if this will cause any issues

					// all vehicle thread to cross intersection
					lock_release(isegCA_lock);
					thread_wakeup(current->sleepAddr);
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

	lock_release(q->head->lock); // Release the dummy head node's lock

	return SUCCESS;
}

/* The main function for the scheduler thread. It schedules vehicles as they approach the intersection. Continues to look until the number of exited cars equals NVEHICLES.

*/
static void  Schedule_vehicles() {

	// I am using a go to here in order to reaquire the lock before checking the condition, after releaseing it during the previous iteration. Though it is often not recommended to use GOTOs, in this situation it seems applicable.
schedule_iteration:
	lock_acquire(numExitedVLock); // TODO: at some point after the threads are created this is no longer null?
	while (numExitedV < NVEHICLES) { // schedule more vehicles
		lock_release(numExitedVLock);
		// TODO: add debug statements here
		if (vehicle_scheduler->A->size == 0 && vehicle_scheduler->C->size == 0 && vehicle_scheduler->T->size == 0 && waiting_zone->A->size == 0 && waiting_zone->C->size == 0 && waiting_zone->T->size == 0) {
			// this could technical be susceptible to a race condition but it doesn't really matter, we just need to quicky wait for some threads to arrive
			goto schedule_iteration;
		}

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
    (void) v; // silence warnings
	// acquire iseg_lock then wake up scheduler effectively notifying it that it entered the intersection 
	// print when vehicle enters and exits
	// if (v->entrance == A) {			// acquire isegAB_lock

	// } else if (v->entrance == B) {	// acquire isegBC_lock
		
	// } else if (v->entrance == C) {	// acquire isegCA_lock

	// }
	
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
    (void) v; // silence warnings
	// int exit;
	// //calculate exit
	// if(v->entrance == 0){exit = 2;}
	// else{exit = v->entrance - 1;}
	// //add the second critical section required
	// v->intersection_segment_required = 7-2^(exit);  // TODO: Is this really needed at all?
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
static void approachintersection(void * unusedpointer, unsigned long vehiclenumber) {
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
	kprintf(formatVehicleMessage(v, "Vehicle Arrived: ")); // TODO: This sting is what is causing the thread panic but I don't know why

	// thread sleep. wait to be woken up by scheduler
	thread_sleep(v->sleepAddr);
	
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

	// print vehicle, exited vehicle
	DEBUG(DB_THREADS, "Exited Intersection Completely: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
	kprintf("Vehicle Exited Intersection Completely: {Vehicle ID: %lu, Vehicle Type: %d, Vehicle Direction: %d, Turn Direction: %d }", v->vehiclenumber, v->vehicle_type, v->entrance, v->turndirection);
	// release vehicle lock
	lock_release(v->lock);
	Vehicle_free(v);

	lock_acquire(numExitedVLock);
	numExitedV++;
	lock_release(numExitedVLock);
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
	kprintf("createvehicles started kprintf\n");
	int index, error;
	index = 0; // use in thread for scheduler too
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
	assert(numExitedVLock != NULL);

	vehicle_scheduler = mlq_init();
	waiting_zone = mlq_init();

	// TODO: Set up the scheduler thread, scheduler thread remains until NVEHICLES have exited the intersection

	// create the vehicle scheduler thread
	error = thread_fork("scheduler thread", NULL, index, Schedule_vehicles,NULL); // TODO: where index = 0, somewhat confused what index i.e. data2 is used for
	if (error) {
		panic("scheduler: thread_fork failed: %s\n",strerror(error)); 
	}
		
	/*
	 * Start NVEHICLES approachintersection() threads.
	 */
	for (index = 1; index <= NVEHICLES; index++) {

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
