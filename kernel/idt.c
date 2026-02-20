#include "idt.h"
#include "io.h"
#include "string.h"
#include "vga.h"

/* ============================================================
 * IDT structures
 * ============================================================ */

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

/* IRQ handler table (16 IRQs: 0-15) */
static isr_handler_t irq_handlers[16] = { 0 };

static const char *exception_messages[32] = {
    "Division By Zero",        "Debug",
    "Non Maskable Interrupt",  "Breakpoint",
    "Overflow",                "Bound Range Exceeded",
    "Invalid Opcode",          "Device Not Available",
    "Double Fault",            "Coprocessor Segment Overrun",
    "Invalid TSS",             "Segment Not Present",
    "Stack-Segment Fault",     "General Protection Fault",
    "Page Fault",              "Reserved",
    "x87 Floating-Point",      "Alignment Check",
    "Machine Check",           "SIMD Floating-Point",
    "Virtualization",          "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved",
    "Hypervisor Injection",    "VMM Communication",
    "Security Exception",      "Reserved"
};

/* ============================================================
 * External ISR/IRQ stubs from interrupt.asm
 * ============================================================ */

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

/* ============================================================
 * IDT gate setup
 * ============================================================ */

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector  = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

/* ============================================================
 * PIC remapping: IRQ 0-15 → INT 32-47
 * ============================================================ */

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

static void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();   /* IRQ 0-7  → INT 32-39 */
    outb(PIC2_DATA, 0x28); io_wait();   /* IRQ 8-15 → INT 40-47 */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/* ============================================================
 * PIT Timer
 * ============================================================ */

#define PIT_CMD  0x43
#define PIT_CH0  0x40
#define PIT_FREQ 1193180

static volatile uint32_t timer_ticks = 0;
static void (*scheduler_fn)(void) = NULL;

static void timer_handler(registers_t *regs) {
    (void)regs;
    timer_ticks++;

    /* Call scheduler if registered (after EOI was already sent) */
    if (scheduler_fn) {
        scheduler_fn();
    }
}

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQ / frequency;
    outb(PIT_CMD, 0x34);  /* Channel 0, lo/hi byte, rate generator */
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    irq_install_handler(0, timer_handler);
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_set_scheduler(void (*callback)(void)) {
    scheduler_fn = callback;
}

/* ============================================================
 * Main ISR handler (called from assembly isr_common_stub)
 * ============================================================ */

void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        /* CPU exception */
        terminal_print_colored("\n*** EXCEPTION: ", VGA_WHITE, VGA_RED);
        terminal_print_colored(exception_messages[regs->int_no], VGA_WHITE, VGA_RED);
        terminal_print_colored(" ***\n", VGA_WHITE, VGA_RED);
        terminal_printf("  INT=%d  ERR=0x%x  EIP=0x%x  CS=0x%x\n",
            regs->int_no, regs->err_code, regs->eip, regs->cs);
        cli();
        for (;;) hlt();
    } else if (regs->int_no >= 32 && regs->int_no < 48) {
        /* Hardware IRQ */
        int irq = regs->int_no - 32;

        /*
         * Send EOI BEFORE calling the handler.
         * This is critical: the timer handler may call schedule(),
         * which context-switches and never returns to this function.
         * Without early EOI, the PIC would never get acknowledged.
         */
        if (irq >= 8) {
            outb(PIC2_CMD, 0x20);
        }
        outb(PIC1_CMD, 0x20);

        if (irq_handlers[irq]) {
            irq_handlers[irq](regs);
        }
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

void irq_install_handler(int irq, isr_handler_t handler) {
    if (irq >= 0 && irq < 16) irq_handlers[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) irq_handlers[irq] = NULL;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;
    memset(&idt, 0, sizeof(idt));

    pic_remap();

    /* Exceptions (INT 0-31): 0x08=kernel CS, 0x8E=present+ring0+interrupt gate */
    idt_set_gate(0,  (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    /* IRQs (INT 32-47) */
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    __asm__ volatile("lidt %0" : : "m"(idtp));
    sti();
}
