/*
 * Core thread system.
 */
#include <types.h>
#include <lib.h>
#include <kern/limits.h>
#include <kern/errno.h>
#include <array.h>
#include <machine/spl.h>
#include <machine/pcb.h>
#include <thread.h>
#include <curthread.h>
#include <scheduler.h>
#include <addrspace.h>
#include <vnode.h>
#include "opt-synchprobs.h"
// #include <pid.h>

/* States a thread can be in. */
typedef enum {
	S_RUN,
	S_READY,
	S_SLEEP,
	S_ZOMB,
} threadstate_t;

/* Global variable for the thread currently executing at any given time. */
struct thread *curthread;

/* Table of sleeping threads. */
static struct array *sleepers;

/* List of dead threads to be disposed of. */
static struct array *zombies;

/* Total number of outstanding threads. Does not count zombies[]. */
static int numthreads;

static struct array *ttable;	// thread table

// Process stuff
static struct array *ptable;	// process table

/*
 * Create a thread. This is used both to create the first thread's 
 * thread structure and to create subsequent threads.
 */

static
struct thread *
thread_create(const char *name, pid_t tid)
{
	// a tid of 0 indicates that this thread struct is for a process

	struct thread *thread = kmalloc(sizeof(struct thread));
	if (thread==NULL) {
		return NULL;
	}
	thread->t_name = kstrdup(name);
	if (thread->t_name==NULL) {
		kfree(thread);
		return NULL;
	}
	thread->tid = tid;

	thread->t_sleepaddr = NULL;
	thread->t_stack = NULL;
	
	thread->t_vmspace = NULL;

	thread->t_cwd = NULL;
	thread->t_process = NULL;
	
	// If you add things to the thread structure, be sure to initialize
	// them here.
	DEBUG(DB_THREADS, "Thread Created\n");
	return thread;
}

/*
 * Destroy a thread.
 *
 * This function cannot be called in the victim thread's own context.
 * Freeing the stack you're actually using to run would be... inadvisable.
 */
static
void
thread_destroy(struct thread *thread)
{
	assert(thread != curthread);

	// If you add things to the thread structure, be sure to dispose of
	// them here or in thread_exit.

	// These things are cleaned up in thread_exit.
	assert(thread->t_vmspace==NULL);
	assert(thread->t_cwd==NULL);
	assert(thread->t_process==NULL);
	
	if (thread->t_stack) {
		kfree(thread->t_stack);
	}

	kfree(thread->t_name);
	kfree(thread);
	DEBUG(DB_THREADS, "Thread Destoryed\n");
}


/*
 * Remove zombies. (Zombies are threads/processes that have exited but not
 * been fully deleted yet.)
 */
static
void
exorcise(void)
{
	int i, result;

	assert(curspl>0);
	
	for (i=0; i<array_getnum(zombies); i++) {
		struct thread *z = array_getguy(zombies, i);
		assert(z!=curthread);
		thread_destroy(z);
	}
	result = array_setsize(zombies, 0);
	/* Shrinking the array; not supposed to be able to fail. */
	assert(result==0);
	DEBUG(DB_THREADS, "Exorcised (Removed) Zombie Threads\n");
}

/*
 * Kill all sleeping threads. This is used during panic shutdown to make 
 * sure they don't wake up again and interfere with the panic.
 */
static
void
thread_killall(void)
{
	int i, result;

	assert(curspl>0);

	/*
	 * Move all sleepers to the zombie list, to be sure they don't
	 * wake up while we're shutting down.
	 */

	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		kprintf("sleep: Dropping thread %s\n", t->t_name);

		/*
		 * Don't do this: because these threads haven't
		 * been through thread_exit, thread_destroy will
		 * get upset. Just drop the threads on the floor,
		 * which is safer anyway during panic.
		 *
		 * array_add(zombies, t);
		 */
	}

	result = array_setsize(sleepers, 0);
	/* shrinking array: not supposed to fail */
	assert(result==0);
	DEBUG(DB_THREADS, "Killed all sleeping threads\n");
}

/*
 * Shut down the other threads in the thread system when a panic occurs.
 */
void
thread_panic(void)
{
	assert(curspl > 0);

	thread_killall();
	scheduler_killall();
}

/*
 * Thread initialization.
 */
struct thread *
thread_bootstrap(void)
{
	struct thread *me;

	/* Create the data structures we need. */
	sleepers = array_create();
	if (sleepers==NULL) {
		panic("Cannot create sleepers array\n");
	}

	zombies = array_create();
	if (zombies==NULL) {
		panic("Cannot create zombies array\n");
	}

	ttable = ttable_init();
	if (ttable == NULL) {
		panic("thread_bootstrap: thread table can not be initialized\n");
	}
	
	/*
	 * Create the thread structure for the first thread
	 * (the one that's already running)
	 */
	pid_t tid = tid_assign();
	me = thread_create("<boot/menu>", tid);
	if (me==NULL) {
		panic("thread_bootstrap: Out of memory\n");
	}

	ptable = ptable_init();
	if (ptable == NULL) {
		panic("thread_bootstrap: process table can not be initialized\n");
	}

	/* Create the initial thread also as a process */
	pid_t pid = pid_assign();
	me->t_process = process_create(pid, 0); // parent process has the same id as itself. Not sure how else to do this?
	if (me->t_process == NULL) {
		panic("Could not create process\n");
	}

	/*
	 * Leave me->t_stack NULL. This means we're using the boot stack,
	 * which can't be freed.
	 */

	/* Initialize the first thread's pcb */
	md_initpcb0(&me->t_pcb);

	/* Set curthread */
	curthread = me;

	/* Number of threads starts at 1 */
	numthreads = 1;
	
	if (ptable==NULL) {
		panic("Could not create process table\n");
	}

	DEBUG(DB_THREADS, "Thread Initialized\n");
	/* Done */
	return me;
}

/*
 * Thread final cleanup.
 */
void
thread_shutdown(void)
{
	array_destroy(sleepers);
	sleepers = NULL;
	array_destroy(zombies);
	zombies = NULL;
	array_destroy(ttable);
	ttable = NULL;
	array_destroy(ptable);
	ptable = NULL;
	// Don't do this - it frees our stack and we blow up
	//thread_destroy(curthread);
}

/*
 * Create a new thread based on an existing one.
 * The new thread has name NAME, and starts executing in function FUNC.
 * DATA1 and DATA2 are passed to FUNC.
 */
int
thread_fork(const char *name, 
	    void *data1, unsigned long data2,
	    void (*func)(void *, unsigned long),
	    struct thread **ret)
{
	struct thread *newguy;
	int s, result;

	// /* Interrupts off for atomicity */
	// s = splhigh();

	/* Allocate a thread */
	pid_t tid = tid_assign();
	newguy = thread_create(name, tid);
	if (newguy==NULL) {
		return ENOMEM;
	}

	/* Allocate a stack */
	newguy->t_stack = kmalloc(STACK_SIZE);
	if (newguy->t_stack==NULL) {
		kfree(newguy->t_name);
		kfree(newguy);
		return ENOMEM;
	}

	/* stick a magic number on the bottom end of the stack */
	newguy->t_stack[0] = 0xae;
	newguy->t_stack[1] = 0x11;
	newguy->t_stack[2] = 0xda;
	newguy->t_stack[3] = 0x33;

	/* Inherit the current directory */
	if (curthread->t_cwd != NULL) {
		VOP_INCREF(curthread->t_cwd);
		newguy->t_cwd = curthread->t_cwd;
	}

	/* Interrupts off for atomicity */
	s = splhigh();

	/* Set up the pcb (this arranges for func to be called) */
	md_initpcb(&newguy->t_pcb, newguy->t_stack, data1, data2, func);

	/* Interrupts off for atomicity */
	s = splhigh();

	/*
	 * Make sure our data structures have enough space, so we won't
	 * run out later at an inconvenient time.
	 */
	result = array_preallocate(sleepers, numthreads+1);
	if (result) {
		goto fail;
	}
	result = array_preallocate(zombies, numthreads+1);
	if (result) {
		goto fail;
	}

	/* Do the same for the scheduler. */
	result = scheduler_preallocate(numthreads+1);
	if (result) {
		goto fail;
	}

	/* Make the new thread runnable */
	result = make_runnable(newguy);
	if (result != 0) {
		goto fail;
	}

	/*
	 * Increment the thread counter. This must be done atomically
	 * with the preallocate calls; otherwise the count can be
	 * temporarily too low, which would obviate its reason for
	 * existence.
	 */
	numthreads++;

	/* Done with stuff that needs to be atomic */
	splx(s);

	/*
	 * Return new thread structure if it's wanted.  Note that
	 * using the thread structure from the parent thread should be
	 * done only with caution, because in general the child thread
	 * might exit at any time.
	 */
	if (ret != NULL) {
		*ret = newguy;
	}

	return 0;

 fail:
	splx(s);
	if (newguy->t_cwd != NULL) {
		VOP_DECREF(newguy->t_cwd);
	}
	kfree(newguy->t_stack);
	kfree(newguy->t_name);
	kfree(newguy);

	return result;
}

/*
 * High level, machine-independent context switch code.
 */
static
void
mi_switch(threadstate_t nextstate)
{
	struct thread *cur, *next;
	int result;
	
	/* Interrupts should already be off. */
	assert(curspl>0);

	if (curthread != NULL && curthread->t_stack != NULL) {
		/*
		 * Check the magic number we put on the bottom end of
		 * the stack in thread_fork. If these assertions go
		 * off, it most likely means you overflowed your stack
		 * at some point, which can cause all kinds of
		 * mysterious other things to happen.
		 */
		assert(curthread->t_stack[0] == (char)0xae);
		assert(curthread->t_stack[1] == (char)0x11);
		assert(curthread->t_stack[2] == (char)0xda);
		assert(curthread->t_stack[3] == (char)0x33);
	}
	
	/* 
	 * We set curthread to NULL while the scheduler is running, to
	 * make sure we don't call it recursively (this could happen
	 * otherwise, if we get a timer interrupt in the idle loop.)
	 */
	if (curthread == NULL) {
		return;
	}
	cur = curthread;
	curthread = NULL;

	/*
	 * Stash the current thread on whatever list it's supposed to go on.
	 * Because we preallocate during thread_fork, this should not fail.
	 */

	if (nextstate==S_READY) {
		result = make_runnable(cur);
	}
	else if (nextstate==S_SLEEP) {
		/*
		 * Because we preallocate sleepers[] during thread_fork,
		 * this should never fail.
		 */
		result = array_add(sleepers, cur);
	}
	else {
		assert(nextstate==S_ZOMB);
		result = array_add(zombies, cur);
	}
	assert(result==0);

	/*
	 * Call the scheduler (must come *after* the array_adds)
	 */

	next = scheduler();

	/* update curthread */
	curthread = next;
	
	/* 
	 * Call the machine-dependent code that actually does the
	 * context switch.
	 */
	md_switch(&cur->t_pcb, &next->t_pcb);
	DEBUG(DB_THREADS, "Completed context switch\n");
	
	/*
	 * If we switch to a new thread, we don't come here, so anything
	 * done here must be in mi_threadstart() as well, or be skippable,
	 * or not apply to new threads.
	 *
	 * exorcise is skippable; as_activate is done in mi_threadstart.
	 */

	exorcise();

	if (curthread->t_vmspace) {
		as_activate(curthread->t_vmspace);
	}
}

/*
 * Cause the current thread to exit.
 *
 * We clean up the parts of the thread structure we don't actually
 * need to run right away. The rest has to wait until thread_destroy
 * gets called from exorcise().
 */
void
thread_exit(void)
{
	if (curthread->t_stack != NULL) {
		/*
		 * Check the magic number we put on the bottom end of
		 * the stack in thread_fork. If these assertions go
		 * off, it most likely means you overflowed your stack
		 * at some point, which can cause all kinds of
		 * mysterious other things to happen.
		 */
		assert(curthread->t_stack[0] == (char)0xae);
		assert(curthread->t_stack[1] == (char)0x11);
		assert(curthread->t_stack[2] == (char)0xda);
		assert(curthread->t_stack[3] == (char)0x33);
		DEBUG(DB_THREADS, "Thread Exited\n");
	}

	splhigh();

	if (curthread->t_vmspace) {
		/*
		 * Do this carefully to avoid race condition with
		 * context switch code.
		 */
		struct addrspace *as = curthread->t_vmspace;
		curthread->t_vmspace = NULL;
		as_destroy(as);
	}

	if (curthread->t_cwd) {
		VOP_DECREF(curthread->t_cwd);
		curthread->t_cwd = NULL;
	}

	assert(numthreads>0);
	numthreads--;
	mi_switch(S_ZOMB);

	panic("Thread came back from the dead!\n");
}

/*
 * Yield the cpu to another process, but stay runnable.
 */
void
thread_yield(void)
{
	int spl = splhigh();

	/* Check sleepers just in case we get here after shutdown */
	assert(sleepers != NULL);

	mi_switch(S_READY);
	splx(spl);
}

/*
 * Yield the cpu to another process, and go to sleep, on "sleep
 * address" ADDR. Subsequent calls to thread_wakeup with the same
 * value of ADDR will make the thread runnable again. The address is
 * not interpreted. Typically it's the address of a synchronization
 * primitive or data structure.
 *
 * Note that (1) interrupts must be off (if they aren't, you can
 * end up sleeping forever), and (2) you cannot sleep in an 
 * interrupt handler.
 */
void
thread_sleep(const void *addr)
{
	// may not sleep in an interrupt handler
	assert(in_interrupt==0);
	
	curthread->t_sleepaddr = addr;
	mi_switch(S_SLEEP);
	curthread->t_sleepaddr = NULL;
}

/*
 * Wake up one or more threads who are sleeping on "sleep address"
 * ADDR.
 */
void
thread_wakeup(const void *addr)
{
	int i, result;
	
	// meant to be called with interrupts off
	assert(curspl>0);
	
	// This is inefficient. Feel free to improve it.
	
	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		if (t->t_sleepaddr == addr) {
			
			// Remove from list
			array_remove(sleepers, i);
			
			// must look at the same sleepers[i] again
			i--;

			/*
			 * Because we preallocate during thread_fork,
			 * this should never fail.
			 */
			result = make_runnable(t);
			assert(result==0);
		}
	}
}

/*
 * Return nonzero if there are any threads who are sleeping on "sleep address"
 * ADDR. This is meant to be used only for diagnostic purposes.
 */
int
thread_hassleepers(const void *addr)
{
	int i;
	
	// meant to be called with interrupts off
	assert(curspl>0);
	
	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		if (t->t_sleepaddr == addr) {
			return 1;
		}
	}
	return 0;
}

// int* process_bootstrap(void) {
// 	int pt_code = init_ptable(ptable);
// 	if (pt_code == -1) {
// 		// ptable init failed
// 		// kprintf("scheduler: Could not create process table");
// 		panic("scheduler: Could not create process table\n");
// 		// return -1;
// 		return NULL;
// 	}
// 	// return 0;
// 	return ptable;
// }

/*
 * New threads actually come through here on the way to the function
 * they're supposed to start in. This is so when that function exits,
 * thread_exit() can be called automatically.
 */
void
mi_threadstart(void *data1, unsigned long data2, 
	       void (*func)(void *, unsigned long))
{
	/* If we have an address space, activate it */
	if (curthread->t_vmspace) {
		as_activate(curthread->t_vmspace);
	}

	/* Enable interrupts */
	spl0();

#if OPT_SYNCHPROBS
	/* Yield a random number of times to get a good mix of threads */
	{
		int i, n;
		n = random()%161 + random()%161;
		for (i=0; i<n; i++) {
			thread_yield();
		}
	}
#endif 
	
	/* Call the function */
	func(data1, data2);

	/* Done. */
	thread_exit();
}

struct array *ttable_init(void) {
	struct array *t = array_create();
	if (t == NULL) {
		panic("Failed to create ptable");
	}

	if (array_preallocate(t, THREAD_MAX) != 0) {
		panic("Could not preallocate ptable");
	}

	// initialize all indexes to -1
	int i;
	for(i = 0; i <= THREAD_MAX; i++) {
        if(array_getguy(t, i) == (void *)(intptr_t) -1) {
			// TODO: If this does not return 0 then there is an error, panic here or propogate the error up
			array_add(t, (void *)(intptr_t)(-1)); // 0 means it is allocated, the index is the actual pid
		}
    }
	// idx 0 is reserved. tid 1 is the main thread.
	array_setguy(t, 0, (void *)(intptr_t)(0));

	return t;
}

/*  Assign a tid to a thread / process. This should ONLY be called with interrupts off. */

pid_t tid_assign(void) {
    
    int i;
    for(i = 0; i <= THREAD_MAX; i++) {
        if(array_getguy(ttable, i) == (void *)(intptr_t) -1) {
			array_setguy(ttable, i, (void *)(intptr_t)(0)); // 0 means it is allocated, the index is the actual pid
            return (pid_t) i;	// pid_t is int32_t and int is int32 in OS161 so this should be safe
		}    
    }

    // max number of processes hit
    return -1;

}



/************************************************************/
/* Explicitly Process Related Code                          */
/************************************************************/

/*
    The thread system predates the process system in OS161. Therefore, the process system utilizes the thread system signficiantly.
    The process system is initialized and destroyed when the thread system is as can be seen in thread_create and thread_destroy.
*/


/*
 *  Creates a new child process.
 *  As a part of this process, the system has to create a new thread in the system but with some differences to thread_fork.
 *      Create a new thread based on an existing one.
 *      The new thread has name NAME, and starts executing in function FUNC.
 *      DATA1 and DATA2 are passed to FUNC.
 *  The difference is that the existing stack is copied to the newly allocated stack and the address space is duplicated.
 */
// int tprocess_fork(const char *name, struct thread **ret) {
// 	struct thread *newguy;
// 	int s, result;

//     /* Interrupts off for atomicity. Interrupts have to be turned off much earlier here then thread_fork since the parent processes stack needs to be copied to this thread if it exists. */
// 	s = splhigh();

//     // TODO: check if the max number of processes has already been created
// 	if (ptable)

// 	/* Allocate a thread */
// 	newguy = thread_create(name, (pid_t) 0);
// 	if (newguy==NULL) {
// 		return ENOMEM;
// 	}

// 	/* Allocate a stack */
// 	newguy->t_stack = kmalloc(STACK_SIZE);
// 	if (newguy->t_stack==NULL) {
// 		kfree(newguy->t_name);
// 		kfree(newguy);
// 		return ENOMEM;
// 	}

//     /* Copy the stack if this is a child process */
//     if (curthread->t_stack != NULL && should_copy_stack()) {
//         memcpy(newguy->t_stack, curthread->t_stack, STACK_SIZE);
        
//         /* Adjust internal stack pointers */
//         adjust_stack_pointers(newguy->t_stack, STACK_SIZE, curthread->t_stack);
//     }

// 	/* stick a magic number on the bottom end of the stack */ // TODO: how should this be handled in the initial process?
// 	// since the process is created off of another process it should already have this. TODO: What about for the first process
// 	newguy->t_stack[0] = 0xae;
// 	newguy->t_stack[1] = 0x11;
// 	newguy->t_stack[2] = 0xda;
// 	newguy->t_stack[3] = 0x33;

// 	/* Inherit the current directory */
// 	if (curthread->t_cwd != NULL) {
// 		VOP_INCREF(curthread->t_cwd);
// 		newguy->t_cwd = curthread->t_cwd;
// 	}

//     // create the pid struct TODO: what to do on initial process creation
// 	newguy->t_process = process_create(pid_assign(), curthread->t_process->pid);
// 	if (newguy->t_process == NULL) {
// 		panic("Could not create process\n"); // TODO: Handle this gracefully?
// 	}

// 	/* Set up the pcb (this arranges for func to be called) */
// 	// md_initpcb(&newguy->t_pcb, newguy->t_stack, data1, data2, func); // this is not needed in the process implementation.

// 	/*
// 	 * Make sure our data structures have enough space, so we won't
// 	 * run out later at an inconvenient time.
// 	 */
// 	result = array_preallocate(sleepers, numthreads+1); //TODO: should sleepers and zombies be moved to thread.h, should a get function be created or should this code move to thread.c
// 	if (result) {
// 		goto fail;
// 	}
// 	result = array_preallocate(zombies, numthreads+1);
// 	if (result) {
// 		goto fail;
// 	}

// 	/* Do the same for the scheduler. */
// 	result = scheduler_preallocate(numthreads+1);
// 	if (result) {
// 		goto fail;
// 	}

// 	/* Make the new thread runnable */
// 	result = make_runnable(newguy);
// 	if (result != 0) {
// 		goto fail;
// 	}

// 	/*
// 	 * Increment the thread counter. This must be done atomically
// 	 * with the preallocate calls; otherwise the count can be
// 	 * temporarily too low, which would obviate its reason for
// 	 * existence.
// 	 */
// 	numthreads++;
// 	array_add(curthread->t_process->children, (void *)(intptr_t)(newguy->t_process->pid)); // add pid to parent's children array

// 	/* Done with stuff that needs to be atomic */
// 	splx(s);

// 	/*
// 	 * Return new thread structure if it's wanted.  Note that
// 	 * using the thread structure from the parent thread should be
// 	 * done only with caution, because in general the child thread
// 	 * might exit at any time.
// 	 */
// 	if (ret != NULL) {
// 		*ret = newguy;
// 	}

// 	return 0;
// 	// TODO: modify fail to handle process struct
//  fail:
// 	splx(s);
// 	if (newguy->t_cwd != NULL) {
// 		VOP_DECREF(newguy->t_cwd);
// 	}
// 	kfree(newguy->t_stack);
// 	kfree(newguy->t_name);
// 	if (newguy->t_process != NULL) {
// 		process_destroy(newguy->t_process);
// 	}

// 	kfree(newguy);

// 	return result;
// }

// int result = process_fork("Child process", child_tf, (unsigned long)child_as, md_forkentry, &child_thread);
int process_fork(const char *name, 
	    void *data1, unsigned long data2,
	    void (*func)(void *, unsigned long),
	    struct thread **ret) {
	struct thread *newguy;
	int s, result;

    /* Interrupts off for atomicity. Interrupts have to be turned off much earlier here then thread_fork since the parent processes stack needs to be copied to this thread if it exists. */
	s = splhigh();

    // TODO: check if the max number of processes has already been created
	// if (ptable->num > PROCESS_MAX) {
	// 	return -1; // TODO this probably has to be changed
	// }
	if (ptable->num >= PROCESS_MAX) {
        return EAGAIN; // return an appropriate error such as ENPROC for too many processes
    }

	/* Allocate a thread */
	newguy = thread_create(name, (pid_t) 0);	// new process has no parent thread
	if (newguy==NULL) {
		return ENOMEM;
	}

	/* Allocate a stack */
	newguy->t_stack = kmalloc(STACK_SIZE);
	if (newguy->t_stack==NULL) {
		kfree(newguy->t_name);
		kfree(newguy);
		return ENOMEM;
	}

	/* stick a magic number on the bottom end of the stack */ // TODO: how should this be handled in the initial process?
	// since the process is created off of another process it should already have this. TODO: What about for the first process
	newguy->t_stack[0] = 0xae;
	newguy->t_stack[1] = 0x11;
	newguy->t_stack[2] = 0xda;
	newguy->t_stack[3] = 0x33;

	/* Inherit the current directory */
	if (curthread->t_cwd != NULL) {
		VOP_INCREF(curthread->t_cwd);
		newguy->t_cwd = curthread->t_cwd;
	}

    // create the pid struct TODO: what to do on initial process creation
	newguy->t_process = process_create(pid_assign(), curthread->t_process->pid);
	if (newguy->t_process == NULL) {
		panic("Could not create process\n"); // TODO: Handle this gracefully?
	}

	/* Set up the pcb (this arranges for func to be called) */
	md_initpcb(&newguy->t_pcb, newguy->t_stack, data1, data2, func); // this is not needed in the process implementation.

	/*
	 * Make sure our data structures have enough space, so we won't
	 * run out later at an inconvenient time.
	 */
	result = array_preallocate(sleepers, numthreads+1); //TODO: should sleepers and zombies be moved to thread.h, should a get function be created or should this code move to thread.c
	if (result) {
		goto fail;
	}
	result = array_preallocate(zombies, numthreads+1);
	if (result) {
		goto fail;
	}

	/* Do the same for the scheduler. */
	result = scheduler_preallocate(numthreads+1);
	if (result) {
		goto fail;
	}

	/* Make the new thread runnable */
	result = make_runnable(newguy);
	if (result != 0) {
		goto fail;
	}

	/*
	 * Increment the thread counter. This must be done atomically
	 * with the preallocate calls; otherwise the count can be
	 * temporarily too low, which would obviate its reason for
	 * existence.
	 */
	numthreads++;
	array_add(curthread->t_process->children, (void *)(intptr_t)(newguy->t_process->pid)); // add pid to parent's children array

	/* Done with stuff that needs to be atomic */
	splx(s);

	/*
	 * Return new thread structure if it's wanted.  Note that
	 * using the thread structure from the parent thread should be
	 * done only with caution, because in general the child thread
	 * might exit at any time.
	 */
	if (ret != NULL) {
		*ret = newguy;
	}

	return 0;
	// TODO: modify fail to handle process struct
 fail:
	splx(s);
	if (newguy->t_cwd != NULL) {
		VOP_DECREF(newguy->t_cwd);
	}
	kfree(newguy->t_stack);
	kfree(newguy->t_name);
	if (newguy->t_process != NULL) {
		process_destroy(newguy->t_process);
	}

	kfree(newguy);

	return result;
}

/*  Adjusts the stack pointers of the child process to the same offset of the parent process.
    This should be called after the stack has copied to the child process.

    Rebases internal pointers in a copied stack segment. Iterates through each pointer sized location in the new stack and ensures it points to the new corresponding location in the new stack rather than the old stack.

*/
// void adjust_stack_pointers(void *new_stack, size_t stack_size, void *old_stack) { // TODO: Frankly I am not sure if this works as intended
//     intptr_t stack_offset = (char *)new_stack - (char *)old_stack;  // gets the byte offset between the old stack and the new stack. (char *) is used since it is 1 byte since arithmetic on (void*) is not allowed in C.
//     size_t i;
//     // adjust the pointers within the new stack to ensure that they point to addresses in the new stack rather than the old stack
//     for (i = 0; i < stack_size; i += sizeof(void *)) {  // increments by the size of a pointer
//         void **ptr = (void **)((char *)new_stack + i);  // pointer casting and dereferencing
//         if (*ptr >= old_stack && *ptr < (char *)old_stack + stack_size) {   // checks if pointer is within valid range then adjusts it
//             *ptr = (char *)(*ptr) + stack_offset;
//         }
//     }
// }

struct process *process_create(pid_t pid, pid_t ppid) {
    struct process *new_process = kmalloc(sizeof(struct process));
    if (new_process == NULL) {
        return NULL;
    }

    new_process->pid = pid;
    new_process->ppid = ppid;
    new_process->children = array_create();
    if (new_process->children == NULL) {
        kfree(new_process);
        return NULL;
    }

	// TODO: Should exit_status and exit_code be set to pointers? 
    new_process->exit_status = 0;
    new_process->exit_code = 0;

    return new_process;
}

void process_destroy(struct process *proc) {
    if (proc != NULL) {
        if (proc->children != NULL) {
            array_destroy(proc->children);	// TODO: this does not actually destroy the child threads, however, should this array be added to zombies?
        }
        kfree(proc);
    }
}

// functions to deal with the ptable which is of type array

struct array *ptable_init(void) {
	struct array *p = array_create();
	if (p == NULL) {
		panic("Failed to create ptable");
	}

	if (array_preallocate(p, PROCESS_MAX) != 0) {
		panic("Could not preallocate ptable");
	}

	// initialize all indexes to -1
	int i;
	for(i = 0; i <= PROCESS_MAX; i++) {
        if(array_getguy(p, i) == (void *)(intptr_t) -1) {
			array_add(p, (void *)(intptr_t)(-1)); // 0 means it is allocated, the index is the actual pid
		}
    }
	// idx 0 is reserved. pid 1 is the main process. A process with a parent of pid 0 signifies the process does not have a parent
	array_setguy(p, 0, (void *)(intptr_t)(0));

	return p;
}

/*  Assign a pid to a thread / process. This should ONLY be called with interrupts off. */

pid_t pid_assign(void) {
    
    int i;
    for(i = 0; i <= PROCESS_MAX; i++) {
        if(array_getguy(ptable, i) == (void *)(intptr_t) -1) {
			array_setguy(ptable, i, (void *)(intptr_t)(0)); // 0 means it is allocated, the index is the actual pid
            return (pid_t) i;	// pid_t is int32_t and int is int32 in OS161 so this should be safe
		}    
    }

    // max number of processes hit
    return -1;

}

