#include "vga.h"
#include "idt.h"
#include "keyboard.h"
#include "memory.h"
#include "shell.h"
#include "ata.h"
#include "fat.h"
#include "process.h"
#include "io.h"

static void ok(const char *msg) {
    terminal_print("  [");
    terminal_print_colored("OK", VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_print("] ");
    terminal_print(msg);
    terminal_print("\n");
}

void main(void) {
    /* 1. Display */
    terminal_init();
    terminal_print_colored("========================================\n", VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_print_colored("            MyOS v0.2.0                 \n", VGA_YELLOW, VGA_BLACK);
    terminal_print_colored("========================================\n", VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_print("\n");

    /* 2. Interrupts */
    idt_init();
    ok("IDT + PIC initialized");

    /* 3. Timer (100 Hz) */
    pit_init(100);
    ok("PIT timer at 100 Hz");

    /* 4. Keyboard */
    keyboard_init();
    ok("PS/2 keyboard driver");

    /* 5. Physical memory (assume 16 MB) */
    pmm_init(16384);
    terminal_print("  [");
    terminal_print_colored("OK", VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_printf("] Physical memory: %d pages (%d KB free)\n",
        pmm_get_total_pages(), pmm_get_free_pages() * 4);

    /* 6. Paging */
    paging_init();
    ok("Paging enabled (identity-mapped 16 MB)");

    /* 7. Heap */
    heap_init();
    terminal_print("  [");
    terminal_print_colored("OK", VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_printf("] Heap: %d KB available\n", heap_get_free() / 1024);

    /* 8. ATA + FAT disk */
    if (ata_init() && fat_init()) {
        ok("FAT16 filesystem mounted");
    } else {
        terminal_print("  [");
        terminal_print_colored("--", VGA_YELLOW, VGA_BLACK);
        terminal_print("] No FAT disk (ls/cat unavailable)\n");
    }

    /* 9. Multitasking */
    multitasking_init();
    ok("Preemptive multitasking enabled");

    /* 10. Launch shell */
    shell_run();
}
