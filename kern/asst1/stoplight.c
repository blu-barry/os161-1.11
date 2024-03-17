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

//num of V
#define NVEHICLES 30

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
typedef struct Queue {
	Vehicle_t* head;
	Vehicle_t* tail;
} Queue_t;
typedef struct MLQ {
	Queue_t* A;	// ambulances
	Queue_t* C; 	// cars
	Queue_t* T; 	// trucks
} MLQ_t;


//definitions
// functions for V 
Vehicle_t* create_vehicle(unsigned long vehicle_id, VehicleType_t vehicle_type, Direction_t entrance, TurnDirection_t turndirection){
	Vehicle_t* v = kmalloc(sizeof(Vehicle_t)); // TODO: What if the malloc fails?
	if(v==NULL){return;}
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

// queue functions
Queue_t* create_queue(){
	// malloc size undecided
	Queue_t* q = kmalloc(sizeof(Vehicle_t)*100);
<<<<<<< HEAD
=======
	if(q==NULL){return NULL;}
>>>>>>> 97242a103e0d614da9851d81d2b7f24f35010f0d
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

//MLQ
MLQ_t* create_mlq(){
	MLQ_t* mlq = kmalloc(sizeof(Queue_t)*3);
	if(mlq==NULL){return NULL;}
	mlq->A = create_queue();
	return mlq;
}
void free_mlq(MLQ_t* mlq){
	free_queue(mlq->A);
	free_queue(mlq->C);
	free_queue(mlq->T);
	kfree(mlq);
	return;
}
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
//absorb v from wait zone and leave an empty body
void consume_waiting_zone(MLQ_t* wait_zone, MLQ_t* scheduler_mlq){
	queue_extend(scheduler_mlq->A, wait_zone->A);
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
//check if a single v can be added to intersection
int check_fit(int intersection, Vehicle_t* v){
	//when there is no confict return 1
	return(!(v->critical_section_required & intersection));
}
// see if an intersection is full
int full(int intersection){return intersection == 7;}
// remove the v from q and update intersection
void v_founded(Queue_t* q, int* intersection, Vehicle_t* v){
	//update value of intersection indicator
	*intersection |= v->critical_section_required;
	//remove v from q
	dequeue(v, q);
	//unf action that put the v into the intersection

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
void schedule_vehicles(MLQ_t* mlq, int* intersection){
	look_for_v_in_from_q(mlq->A, intersection);
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
//set the critical section of v
static void setturn(Vehicle_t* v){
	if(v->turndirection == 0){turnright(v);}
	else{turnleft(v);}
	return;
}

//approach adds a v into an mlq
static void approach(Vehicle_t *v, MLQ_t* mlq){
	if(v->vehicle_type == 0){enqueue(v,mlq->A);}
	if(v->vehicle_type == 0){enqueue(v,mlq->C);}
	else{enqueue(v,mlq->T);}
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
	unsigned long vid;
	//Avoid unused variable and function warnings. 
	(void) vehiclenumber;
	//entrance is set randomly.
	entrance = random() % 3;
	turndirection = random() % 2;
	vehicletype = random() % 3;
	vid = pthread_self();
	// thread has been created

	// create vehicle and set turn
	Vehicle_t * v = create_vehicle(vid,vehicletype,entrance,turndirection);
	setturn(v);
	// insert into waiting zone MLQ i.e. approached the intersection
	approach(v, mlq);

	// print state
	print_vehicle(v);
	// thread sleep
	//unf

	// turn here after thread wake up occurs
	// unf
}

// scheduler
void scheduler(){
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

int createvehicles(int nargs, char ** args){
	int index, error;
	MLQ_t* mlq = mlq_create();
	/*
	 * Avoid unused variable warnings.
	 */
	(void) nargs;
	(void) args;
	
	// TODO: Set up the scheduler thread
	error = thread_fork("scheduler thread",NULL, index, scheduler,NULL);
	if (error) {panic("scheduler: thread_fork failed: %s\n",strerror(error));}

		
	//Start NVEHICLES approachintersection() threads.
	for (index = 0; index < NVEHICLES; index++) {
		//mutex lock unf
		//condition wait until lock available
		while(/*lock unavailable unf*/ 1==1){}
		//create v with approach intersection
		error = thread_fork("approachintersection thread",
			mlq,index,approachintersection,NULL);
		//panic() on error.
		if (error) {
			panic("approachintersection: thread_fork failed: %s\n",
			strerror(error));}
		//mutex unlock	
	}

	return 0;
}
