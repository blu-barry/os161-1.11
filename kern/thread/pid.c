/*
 * Core process system.
 */

#include <types.h>
#include <pid.h>
#include <lib.h>
#include <thread.h>
#include <scheduler.h>
#include <kern/limits.h>

// TODO: This is the file where the process table functions should go

/*  Initializes the process table (of size PROCESS_MAX) full of -1. 
    NOTE: This function should only be run once and is called in `~/os161-1.11/kern/thread/scheduler.c`.

    ARGS:
        ptable (int*): Pointer for the process table.
    RETURNS:
        0 on success and -1 on failure.
 */

int init_ptable(int* ptable) {
    *ptable = kmalloc(sizeof(int) * PROCESS_MAX);

    // check if kmalloc fails
    if(ptable == NULL) {
        return -1;
    }

    // fill ptable with -1
    int i;
    for (i = 0; i < PROCESS_MAX; i++) {
        ptable[i] = -1;
    }

    return 0;
}

// assign a pid to a thread / process
int pid_assign() {

    int *ptable = get_ptable(); // func in scheduler.c
    
    int i;
    for(i = 0; i < PROCESS_MAX; i++) {
        if(ptable[i] == -1)
            ptable[i] = 0; // 0 means it is allocated, the index is the actual pid
            return i;
    }

    // max number of processes hit
    return -1;

}

