#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <types.h>

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);
pid_t sys_getpid(int code);


#endif /* _SYSCALL_H_ */
