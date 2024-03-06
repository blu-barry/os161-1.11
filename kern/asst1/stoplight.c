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


//constants

//num of V
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

typedef enum TurnDirection {
    R = 0,
	L = 1
} TurnDirection_t;

typedef struct Vehicle {
	// thread_id ?
	unsigned long vehicle_id;
	VehicleType_t vehicle_type;
	Direction_t vehicledirection;
	TurnDirection_t turndirection;
	struct Vehicle* next;
} Vehicle_t;

typedef struct Queue {
	Vehicle_t* head;
	Vehicle_t* tail;
} Queue_t;

typedef struct MLQ {
	// TODO implement multi level queue
	Queue_t A;	// ambulances
	Queue_t C; 	// cars
	Queue_t T; 	// trucks
} MLQ_t;

//definitions

/* functions for V */
Vehicle_t* create_vehicle(unsigned long vehicle_id, VehicleType_t vehicle_type, Direction_t vehicledirection, TurnDirection_t turndirection){
	Vehicle_t* v = kmalloc(sizeof(Vehicle_t));
	v->vehicle_id = vehicle_id;
	v->vehicle_type = vehicle_type;
	v->vehicledirection = vehicledirection;
	v->turndirection = turndirection;
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
	printf(	"Vehicle Direction: %d\n",v->vehicledirection);
	printf("Turn Direction: %d\n",v->turndirection);
	return;
}

// queue functions
Queue_t* create_queue(){
	// malloc size undecided
	Queue_t* q = kmalloc(sizeof(Vehicle_t)*100);
	q->head = NULL;
	q->tail = NULL;
	return q;
}
void free_queue(Queue_t* q){
	free_vehicle(q->head);
}
void enqueue(Vehicle_t * v, Queue_t* q){
	if(q->head == NULL){
		q->head = v;
		q->tail = v;
	} else {
		q->tail->next = v;
		q->tail = v;
	}
}
void dequeue(Vehicle_t * v, Queue_t* q){
	//if null Q
	if(q->head == NULL){
		printf("null queue\n");
		return;
	}
	Vehicle_t* cur_v = q->head;
	Vehicle_t* v_grab = NULL;
	//found at head
	if (same_vehicle(cur_v, v)){
		v_grab = cur_v;
		q->head = q->head->next;
		return v_grab;
	}
	//loop through to find
	while(cur_v->next != NULL){
		if(same_vehicle(cur_v->next, v)){
			v_grab = cur_v->next;
			cur_v->next = cur_v->next->next;
			v_grab->next = NULL;
			return v_grab;
		}
		cur_v = cur_v->next;
	}
	print("not found\n");
	return NULL;
}
void queue_extend(Queue_t* receiver, Queue_t* sender)// addeds the sender queue to the receiver queue
{
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
/*
input V
based on v->direction, turn->direction
calculate exit it takes and critical section requires.	
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
	Direction_t vehicledirection;
	TurnDirection_t turndirection;
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
