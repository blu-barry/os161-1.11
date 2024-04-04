/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>
// #include <stdbool.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	DEBUG(DB_THREADS, "Semaphore Created\n");
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
	DEBUG(DB_THREADS, "Semaphore Destroyed\n");
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.


int TestAndSet(int *old_ptr, int new) { 		// From the textbook
	int old = *old_ptr; 						// fetch old value at old_ptr
	*old_ptr = new; 							// store ’new’ into old_ptr
	return old; 								// return the old value
}

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
	// add stuff here as needed
	lock->available = 1; 						// true, lock is available // TODO: warning: assignment makes pointer from integer without a cast
	lock->holder = NULL; 						// no thread currenty holds the lock

	// DEBUG(DB_THREADS, "Lock Created\n");
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	// add stuff here as needed

	kfree(lock->available); 					// not needed when volatile instead of pointer
	kfree(lock->holder); 						// TODO: is this freeing the thread itself or just the pointer to the thread??
	kfree(lock->name);
	kfree(lock);
	// DEBUG(DB_THREADS, "Lock Destroyed\n");
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);
	
	int spl = splhigh();
	while(lock->available == 0){
		thread_sleep(lock);
	}
	// warning: comparison between pointer and integer 
	assert(lock->available == 1); 				// double check that the lock is available

	lock->available = 0;
	lock->holder = curthread; 					// unique identifier for the thread

	// DEBUG(DB_THREADS, "Lock Acquired\n");
	splx(spl); 									// TODO: why does it only work when I call splx blocking interrupt at the end
}

// lock_try_acquire_alert is nearly identical to lock_acquire except instead of sleeping if it can't acquire the lock, it returns false. If it can acquire the lock, it returns true. 
int
lock_try_acquire_alert(struct lock *lock) {
	assert(lock != NULL);
	
	if(lock_do_i_hold(lock)) { // already hold the lock
		// return false;
		return 0;
	}

	int spl = splhigh();
	while(lock->available == 0){
		splx(spl); 	
		// DEBUG(DB_THREADS, "Lock Not Acquired\n");
		// return false;
		return 0;
	}
	assert(lock->available == 1); 				// double check that the lock is available

	lock->available = 0;
	lock->holder = curthread; 					// unique identifier for the thread

	// DEBUG(DB_THREADS, "Lock Acquired\n");
	splx(spl); 	
	// return true;	
	return 1;
}

void
lock_release(struct lock *lock)
{
	int spl;
	assert(lock != NULL);
	spl = splhigh();
	if(lock_do_i_hold(lock) == 1){
		lock->available = 1;
		lock->holder = NULL;					// lock is no longer owned by thread
		assert(lock->available == 1);
		thread_wakeup(lock);					// wake up threads waiting on the lock
	}
	splx(spl);									// TODO: I am somewhat confused why preventing interrupts when the lock is acquired works as opposed to at the start
	DEBUG(DB_THREADS, "Lock Released\n");
}

int
lock_do_i_hold(struct lock *lock)
{
	assert(lock != NULL);
	if(lock->holder == curthread){
		return 1;
	}
	else{
		return 0;
	}
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
	// add stuff here as needed
	
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	// add stuff here as needed
	
	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	// Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	// Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}
