#include "process.h"
#include "memory.h"
#include "string.h"
#include "idt.h"
#include "io.h"
#include "vga.h"

/*
 * Preemptive multitasking with round-robin scheduling.
 * The PIT timer interrupt calls schedule() which may switch processes.
 * Each process runs in ring 0 (kernel mode) with its own stack.
 */

static process_t processes[MAX_PROCESSES];
static int current_pid = -1;
static bool mt_enabled = false;

/* Defined in switch.asm */
extern void task_start_wrapper(void);

void multitasking_init(void) {
    memset(processes, 0, sizeof(processes));

    /* Process 0 = the kernel (current execution context) */
    processes[0].pid = 0;
    processes[0].state = PROC_RUNNING;
    processes[0].name = "kernel";
    processes[0].esp = 0;  /* Will be saved on first context switch */
    processes[0].stack_base = 0;  /* Uses original kernel stack */

    current_pid = 0;
    mt_enabled = true;

    /* Register scheduler with the timer interrupt */
    timer_set_scheduler(schedule);
}

int process_create(void (*entry)(void), const char *name) {
    /* Find a free slot */
    int pid = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_UNUSED) {
            pid = i;
            break;
        }
    }
    if (pid == -1) return -1;

    /* Allocate a stack page */
    void *stack = pmm_alloc_page();
    if (!stack) return -1;
    memset(stack, 0, PROCESS_STACK_SIZE);

    uint32_t stack_top = (uint32_t)stack + PROCESS_STACK_SIZE;

    /*
     * Set up the initial stack so context_switch() works:
     *
     * [stack_top]     process_exit    <- return addr for the task function
     * [stack_top - 4] entry           <- return addr for task_start_wrapper
     * [stack_top - 8] task_start_wrapper <- return addr for context_switch's ret
     * [stack_top -12] 0               <- EBP
     * [stack_top -16] 0               <- EDI
     * [stack_top -20] 0               <- ESI
     * [stack_top -24] 0               <- EBX
     *  ^-- ESP points here
     */
    uint32_t *sp = (uint32_t *)stack_top;
    *(--sp) = (uint32_t)process_exit;       /* If task function returns */
    *(--sp) = (uint32_t)entry;              /* task_start_wrapper does ret → here */
    *(--sp) = (uint32_t)task_start_wrapper; /* context_switch ret → here */
    *(--sp) = 0;  /* EBP */
    *(--sp) = 0;  /* EDI */
    *(--sp) = 0;  /* ESI */
    *(--sp) = 0;  /* EBX */

    processes[pid].pid = pid;
    processes[pid].esp = (uint32_t)sp;
    processes[pid].stack_base = (uint32_t)stack;
    processes[pid].state = PROC_READY;
    processes[pid].name = name;

    return pid;
}

void schedule(void) {
    if (!mt_enabled || current_pid < 0) return;

    /* Find next runnable process (round-robin) */
    int next = current_pid;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        next = (next + 1) % MAX_PROCESSES;
        if (processes[next].state == PROC_READY) {
            break;
        }
    }

    /* If same process or no other ready process, don't switch */
    if (next == current_pid) return;
    if (processes[next].state != PROC_READY) return;

    int old_pid = current_pid;
    current_pid = next;

    processes[old_pid].state = (processes[old_pid].state == PROC_RUNNING)
                                ? PROC_READY : processes[old_pid].state;
    processes[next].state = PROC_RUNNING;

    context_switch(&processes[old_pid].esp, processes[next].esp);
}

void process_exit(void) {
    if (current_pid > 0) {
        processes[current_pid].state = PROC_TERMINATED;

        /* Free the stack */
        if (processes[current_pid].stack_base) {
            pmm_free_page((void *)processes[current_pid].stack_base);
        }

        processes[current_pid].state = PROC_UNUSED;
    }

    /* Switch to another process */
    schedule();

    /* If nothing to switch to, halt */
    for (;;) hlt();
}

int process_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_READY || processes[i].state == PROC_RUNNING) {
            count++;
        }
    }
    return count;
}

process_t *process_get_list(void) {
    return processes;
}

uint32_t process_current_pid(void) {
    return (current_pid >= 0) ? (uint32_t)current_pid : 0;
}

bool multitasking_enabled(void) {
    return mt_enabled;
}
