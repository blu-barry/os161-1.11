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


/*
 *
 * Function Definitions
 *
 */



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

static
void
turnleft(unsigned long vehicledirection,
		unsigned long vehiclenumber,
		unsigned long vehicletype)
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

static
void
turnright(unsigned long vehicledirection,
		unsigned long vehiclenumber,
		unsigned long vehicletype)
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

static
void
approachintersection(void * unusedpointer,
		unsigned long vehiclenumber)
{
	int vehicledirection, turndirection, vehicletype;

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

int
createvehicles(int nargs,
		char ** args)
{
	int index, error;

	/*
	 * Avoid unused variable warnings.
	 */

	(void) nargs;
	(void) args;

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
