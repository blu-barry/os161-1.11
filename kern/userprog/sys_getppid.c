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

int sys_getppid(struct trapframe *parent_tf) {
    if (curthread->t_process != NULL) {
        if (curthread->t_process->ppid == 0) { // this is the main process and should never be exited unless the kernel shuts down
            return curthread->t_process->ppid;
        }
        if (has_process_exited(curthread->t_process->ppid)) { // TODO: do interrupts need to be off here?
            return -1;
        }
        return curthread->t_process->ppid;
    }
    
    if (has_process_exited(curthread->thread_group_ppid)) { // TODO: do interrupts need to be off here?
            return -1;
        }
    return curthread->thread_group_ppid;   // the thread group pid is the pid of the parent process in the thread group
}