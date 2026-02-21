#include "vga.h"
#include "font.h"
#include "idt.h"
#include "keyboard.h"
#include "memory.h"
#include "shell.h"
#include "ata.h"
#include "fat.h"
#include "process.h"
#include "io.h"
#include "gdt.h"
#include "bootinfo.h"
#include "fb.h"
#include "mouse.h"
#include "event.h"
#include "wm.h"
#include "syscall.h"

static void ok(const char *msg) {
    terminal_print("  [");
    terminal_print_colored("OK", VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_print("] ");
    terminal_print(msg);
    terminal_print("\n");
}

void main(void) {
    gdt_init();
    pmm_init(16384);
    paging_init();
    fb_init();

    terminal_init();
    if (!fb_is_active()) font_load();

    terminal_print_colored("========================================\n", VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_print_colored("            MyOS v0.3.0                 \n", VGA_YELLOW, VGA_BLACK);
    terminal_print_colored("========================================\n", VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_print("\n");

    ok("GDT + TSS initialized");

    idt_init();
    ok("IDT + PIC initialized");

    pit_init(100);
    ok("PIT timer at 100 Hz");

    keyboard_init();
    ok("PS/2 keyboard driver");

    mouse_init();
    ok("PS/2 mouse driver");

    terminal_print("  [");
    terminal_print_colored("OK", VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_printf("] Physical memory: %d pages (%d KB free)\n",
        pmm_get_total_pages(), pmm_get_free_pages() * 4);

    ok("Paging enabled (identity-mapped 16 MB)");

    if (fb_is_active()) {
        terminal_printf("  [");
        terminal_print_colored("OK", VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_printf("] VESA framebuffer: %dx%d\n", fb_get_width(), fb_get_height());
    }

    heap_init();
    terminal_print("  [");
    terminal_print_colored("OK", VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_printf("] Heap: %d KB available\n", heap_get_free() / 1024);

    if (ata_init() && fat_init()) {
        ok("FAT16 filesystem mounted");
    } else {
        terminal_print("  [");
        terminal_print_colored("--", VGA_YELLOW, VGA_BLACK);
        terminal_print("] No FAT disk (ls/cat unavailable)\n");
    }

    multitasking_init();
    ok("Preemptive multitasking enabled");

    syscall_init();
    ok("Syscall gate (INT 0x80)");

    event_init();

    if (fb_is_active()) {
        ok("Starting window manager...");
        wm_init();
        wm_run();
    } else {
        shell_run();
    }
}
