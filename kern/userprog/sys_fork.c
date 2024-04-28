

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
#include <machine/trapframe.h>

int sys_fork(struct trapframe *parent_tf, int *retval) {
    struct trapframe *child_tf;
    struct thread *child_thread;
    int result;

    // Create a copy of the parent's trapframe on the heap
    child_tf = kmalloc(sizeof(struct trapframe));
    if (child_tf == NULL) {
        return ENOMEM;
    }
    memcpy(child_tf, parent_tf, sizeof(struct trapframe));

    result = thread_fork(curthread->t_name, child_tf, 0, md_forkentry, &child_thread);
    if (result) {
        kfree(child_tf);
        return result;
    }

    // create a new process structure for the child
    if (curthread->t_process) {
        child_thread->t_process = process_create(child_thread->tid, curthread->t_process->pid);
        if (child_thread->t_process == NULL) {
            kfree(child_tf);
            thread_destroy(child_thread); // clean up thread
            return ENOMEM;
        }
    }

    *retval = child_thread->t_process->pid;
    return 0;
}