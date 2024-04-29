#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <types.h>

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);
// pid_t sys_getpid(int code);
int sys_fork(struct trapframe *parent_tf, int *retval);
int sys_getpid(struct trapframe *parent_tf);
int sys_getppid(struct trapframe *parent_tf);   // Return the pid of the parent process of the executing process. • If parent process has exited or does not exist, return -1.


#endif /* _SYSCALL_H_ */
