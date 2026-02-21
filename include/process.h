#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

#define MAX_PROCESSES    8
#define PROCESS_STACK_SIZE 4096

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_TERMINATED
} process_state_t;

typedef struct {
    uint32_t pid;
    uint32_t esp;
    uint32_t stack_base;
    uint32_t kernel_stack;
    uint32_t kernel_stack_top;
    bool     is_user;
    process_state_t state;
    const char *name;
} process_t;

void multitasking_init(void);
int  process_create(void (*entry)(void), const char *name);
int  process_create_user(void (*entry)(void), const char *name);
void schedule(void);
void process_exit(void);
int  process_count(void);
process_t *process_get_list(void);
uint32_t process_current_pid(void);
bool multitasking_enabled(void);

extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

#endif
