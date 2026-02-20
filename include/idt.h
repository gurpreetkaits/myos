#ifndef IDT_H
#define IDT_H

#include "types.h"

/* Interrupt stack frame pushed by CPU + our stubs */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* pusha */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;            /* pushed by CPU */
} registers_t;

typedef void (*isr_handler_t)(registers_t *);

void idt_init(void);
void irq_install_handler(int irq, isr_handler_t handler);
void irq_uninstall_handler(int irq);

/* PIT timer */
void pit_init(uint32_t frequency);
uint32_t timer_get_ticks(void);

/* Scheduler hook: called from the timer interrupt after EOI */
void timer_set_scheduler(void (*callback)(void));

#endif
