#include "syscall.h"
#include "idt.h"
#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "io.h"

void syscall_handler(registers_t *regs) {
    switch (regs->eax) {
        case SYS_EXIT:
            process_exit();
            break;

        case SYS_WRITE: {
            const char *str = (const char *)regs->ebx;
            uint32_t len = regs->ecx;
            for (uint32_t i = 0; i < len; i++) {
                terminal_putchar(str[i]);
            }
            regs->eax = len;
            break;
        }

        case SYS_GETKEY:
            regs->eax = keyboard_read();
            break;

        case SYS_YIELD:
            schedule();
            break;

        default:
            regs->eax = (uint32_t)-1;
            break;
    }
}

void syscall_init(void) {
}
