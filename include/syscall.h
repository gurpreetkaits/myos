#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETKEY  2
#define SYS_YIELD   3

void syscall_init(void);

#endif
