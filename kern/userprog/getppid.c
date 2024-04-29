#include <types.h>
#include <thread.h>
#include <curthread.h> 

// Process stuff
static struct array *ptable

/*
 * Return -1 if the parent has already terminated or does not exist.
 * Else return the parent's process ID 
 */
pid_t getppid(void) {
    // Ensure the current thread has an process structure
    if (curthread == NULL || curthread->t_process == NULL) {
        return -1; // No process context
    }

    pid_t parent_pid = curthread->t_process->ppid;
    
    // Check if the parent PID is still active
    if (parent_pid == 0 || !process_exists(parent_pid)) {
        return -1; // Parent has terminated or does not exist
    }

    return parent_pid; // Return the parent PID
}

/*
 * Helper function to determine if a process with a given PID exists and is still active.
 * This function directly accesses the process table using the PID as an index.
 */
int process_exists(pid_t pid) {
    if (pid > 0 && pid < array_num(ptable)) {
        struct process *proc = array_get(ptable, pid - 1); // Subtract 1 because array is 0-indexed
        if (proc != NULL && proc->pid == pid) {
            return 1; // Process exists and is active
        }
    }
    return 0; // Process does not exist or has terminated
}