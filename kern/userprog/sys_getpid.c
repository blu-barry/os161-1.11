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

int sys_getpid(struct trapframe *parent_tf) {
    if (curthread->t_process != NULL) {
        return curthread->t_process->pid;
    }
    return curthread->thread_group_pid;   // the thread group pid is the pid of the parent process in the thread group
}