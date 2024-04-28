
#include <curthread.h>
#include <thread.h>
#include <types.h>

pid_t sys_getpid (int code) {
    if (curthread->t_process != NULL) {
        return curthread->t_process->pid;
    }
    return -1;  // TODO: this should probably return something else but I am not sure what is appropriate since all threads are not processes
}