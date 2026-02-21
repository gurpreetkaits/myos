#include "process.h"
#include "memory.h"
#include "string.h"
#include "idt.h"
#include "io.h"
#include "vga.h"
#include "gdt.h"

static process_t processes[MAX_PROCESSES];
static int current_pid = -1;
static bool mt_enabled = false;

extern void task_start_wrapper(void);
extern void user_mode_enter(void);

void multitasking_init(void) {
    memset(processes, 0, sizeof(processes));

    processes[0].pid = 0;
    processes[0].state = PROC_RUNNING;
    processes[0].name = "kernel";
    processes[0].esp = 0;
    processes[0].stack_base = 0;

    current_pid = 0;
    mt_enabled = true;

    timer_set_scheduler(schedule);
}

int process_create(void (*entry)(void), const char *name) {
    int pid = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_UNUSED) {
            pid = i;
            break;
        }
    }
    if (pid == -1) return -1;

    void *stack = pmm_alloc_page();
    if (!stack) return -1;
    memset(stack, 0, PROCESS_STACK_SIZE);

    uint32_t stack_top = (uint32_t)stack + PROCESS_STACK_SIZE;

    uint32_t *sp = (uint32_t *)stack_top;
    *(--sp) = (uint32_t)process_exit;
    *(--sp) = (uint32_t)entry;
    *(--sp) = (uint32_t)task_start_wrapper;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;

    processes[pid].pid = pid;
    processes[pid].esp = (uint32_t)sp;
    processes[pid].stack_base = (uint32_t)stack;
    processes[pid].is_user = false;
    processes[pid].state = PROC_READY;
    processes[pid].name = name;

    return pid;
}

int process_create_user(void (*entry)(void), const char *name) {
    int pid = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_UNUSED) {
            pid = i;
            break;
        }
    }
    if (pid == -1) return -1;

    void *user_stack = pmm_alloc_page();
    if (!user_stack) return -1;
    memset(user_stack, 0, PROCESS_STACK_SIZE);

    void *kernel_stack = pmm_alloc_page();
    if (!kernel_stack) {
        pmm_free_page(user_stack);
        return -1;
    }
    memset(kernel_stack, 0, PROCESS_STACK_SIZE);

    uint32_t user_stack_top = (uint32_t)user_stack + PROCESS_STACK_SIZE;
    uint32_t kernel_stack_top = (uint32_t)kernel_stack + PROCESS_STACK_SIZE;

    uint32_t *sp = (uint32_t *)kernel_stack_top;

    *(--sp) = GDT_USER_DATA | 0x03;
    *(--sp) = user_stack_top;
    *(--sp) = 0x202;
    *(--sp) = GDT_USER_CODE | 0x03;
    *(--sp) = (uint32_t)entry;

    *(--sp) = (uint32_t)user_mode_enter;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;

    processes[pid].pid = pid;
    processes[pid].esp = (uint32_t)sp;
    processes[pid].stack_base = (uint32_t)user_stack;
    processes[pid].kernel_stack = (uint32_t)kernel_stack;
    processes[pid].kernel_stack_top = kernel_stack_top;
    processes[pid].is_user = true;
    processes[pid].state = PROC_READY;
    processes[pid].name = name;

    return pid;
}

void schedule(void) {
    if (!mt_enabled || current_pid < 0) return;

    int next = current_pid;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        next = (next + 1) % MAX_PROCESSES;
        if (processes[next].state == PROC_READY) {
            break;
        }
    }

    if (next == current_pid) return;
    if (processes[next].state != PROC_READY) return;

    int old_pid = current_pid;
    current_pid = next;

    processes[old_pid].state = (processes[old_pid].state == PROC_RUNNING)
                                ? PROC_READY : processes[old_pid].state;
    processes[next].state = PROC_RUNNING;

    if (processes[next].is_user) {
        tss_set_kernel_stack(processes[next].kernel_stack_top);
    }

    context_switch(&processes[old_pid].esp, processes[next].esp);
}

void process_exit(void) {
    if (current_pid > 0) {
        processes[current_pid].state = PROC_TERMINATED;

        if (processes[current_pid].stack_base) {
            pmm_free_page((void *)processes[current_pid].stack_base);
        }
        if (processes[current_pid].kernel_stack) {
            pmm_free_page((void *)processes[current_pid].kernel_stack);
        }

        processes[current_pid].state = PROC_UNUSED;
    }

    schedule();
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
