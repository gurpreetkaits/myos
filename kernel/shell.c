#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "memory.h"
#include "fat.h"
#include "process.h"
#include "idt.h"
#include "io.h"

#define CMD_BUF_SIZE 256
#define MAX_ARGS     16

static char cmd_buf[CMD_BUF_SIZE];
static int  cmd_len = 0;

/* ============================================================
 * Command implementations
 * ============================================================ */

static void cmd_help(void) {
    terminal_print_colored("Available commands:\n", VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_print("  help     - Show this help message\n");
    terminal_print("  clear    - Clear the screen\n");
    terminal_print("  reboot   - Reboot the system\n");
    terminal_print("  meminfo  - Show memory information\n");
    terminal_print("  echo     - Print text to screen\n");
    terminal_print("  ls       - List files on disk\n");
    terminal_print("  cat      - Display file contents\n");
    terminal_print("  tasks    - Show running processes\n");
    terminal_print("  demo     - Start multitasking demo\n");
    terminal_print("  uname    - Show system info\n");
}

static void cmd_clear(void) {
    terminal_clear();
}

static void cmd_reboot(void) {
    terminal_print("Rebooting...\n");
    /* Pulse CPU reset line via keyboard controller */
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    for (;;) hlt();
}

static void cmd_meminfo(void) {
    terminal_print_colored("Memory Information:\n", VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_printf("  Physical pages: %d total, %d free (%d KB free)\n",
        pmm_get_total_pages(), pmm_get_free_pages(),
        pmm_get_free_pages() * 4);
    terminal_printf("  Heap: %d bytes used, %d bytes free\n",
        heap_get_used(), heap_get_free());

    /* Quick kmalloc/kfree test */
    void *p = kmalloc(128);
    if (p) {
        terminal_printf("  Alloc test: kmalloc(128) = %x ", (uint32_t)p);
        kfree(p);
        terminal_print_colored("[OK]\n", VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        terminal_print_colored("  Alloc test: FAILED\n", VGA_LIGHT_RED, VGA_BLACK);
    }
}

static void cmd_echo(const char *args) {
    if (args && *args) {
        terminal_print(args);
    }
    terminal_print("\n");
}

static void cmd_ls(void) {
    if (!fat_is_mounted()) {
        terminal_print_colored("No filesystem mounted.\n", VGA_YELLOW, VGA_BLACK);
        terminal_print("Attach a FAT16 disk image as secondary IDE drive.\n");
        return;
    }

    fat_dir_entry_t entries[32];
    int count = fat_list_root(entries, 32);

    if (count < 0) {
        terminal_print_colored("Error reading directory.\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    if (count == 0) {
        terminal_print("(empty directory)\n");
        return;
    }

    terminal_print_colored("Name            Size     Attr\n", VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_print("-------------------------------\n");

    for (int i = 0; i < count; i++) {
        /* Name (padded to 16 chars) */
        terminal_print(entries[i].name);
        int pad = 16 - strlen(entries[i].name);
        while (pad-- > 0) terminal_putchar(' ');

        /* Size */
        if (entries[i].attr & FAT_ATTR_DIRECTORY) {
            terminal_print_colored("<DIR>   ", VGA_LIGHT_BLUE, VGA_BLACK);
        } else {
            char buf[12];
            int_to_str(entries[i].size, buf);
            terminal_print(buf);
            pad = 9 - strlen(buf);
            while (pad-- > 0) terminal_putchar(' ');
        }

        /* Attributes */
        if (entries[i].attr & FAT_ATTR_READONLY) terminal_putchar('R');
        if (entries[i].attr & FAT_ATTR_HIDDEN)   terminal_putchar('H');
        if (entries[i].attr & FAT_ATTR_SYSTEM)   terminal_putchar('S');
        terminal_print("\n");
    }
    terminal_printf("\n%d file(s)\n", count);
}

static void cmd_cat(const char *filename) {
    if (!filename || !*filename) {
        terminal_print("Usage: cat <filename>\n");
        return;
    }

    if (!fat_is_mounted()) {
        terminal_print_colored("No filesystem mounted.\n", VGA_YELLOW, VGA_BLACK);
        return;
    }

    /* Use heap to allocate read buffer */
    uint8_t *buf = (uint8_t *)kmalloc(4096);
    if (!buf) {
        terminal_print_colored("Out of memory.\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    int bytes = fat_read_file(filename, buf, 4095);
    if (bytes < 0) {
        terminal_printf("File not found: %s\n", filename);
    } else {
        buf[bytes] = '\0';
        terminal_print((const char *)buf);
        if (bytes > 0 && buf[bytes - 1] != '\n') terminal_print("\n");
    }

    kfree(buf);
}

static void cmd_tasks(void) {
    if (!multitasking_enabled()) {
        terminal_print("Multitasking not initialized.\n");
        return;
    }

    process_t *list = process_get_list();
    terminal_print_colored("PID  State      Name\n", VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_print("------------------------\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (list[i].state == PROC_UNUSED) continue;

        terminal_printf("%d    ", list[i].pid);

        switch (list[i].state) {
            case PROC_RUNNING:
                terminal_print_colored("RUNNING   ", VGA_LIGHT_GREEN, VGA_BLACK);
                break;
            case PROC_READY:
                terminal_print_colored("READY     ", VGA_YELLOW, VGA_BLACK);
                break;
            case PROC_TERMINATED:
                terminal_print_colored("DONE      ", VGA_DARK_GREY, VGA_BLACK);
                break;
            default:
                terminal_print("???       ");
        }

        terminal_printf("%s\n", list[i].name);
    }
    terminal_printf("\nActive processes: %d\n", process_count());
}

/* Demo tasks for multitasking */
static volatile int demo_running = 0;

static void demo_task_a(void) {
    __asm__ volatile("sti");
    int count = 0;
    while (count < 50) {
        terminal_print_at("Task A: ", 50, 2);
        char buf[12];
        int_to_str(count++, buf);
        terminal_print_colored(buf, VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_print("   ");
        /* Busy wait for visible delay */
        for (volatile int i = 0; i < 500000; i++);
    }
    demo_running--;
}

static void demo_task_b(void) {
    __asm__ volatile("sti");
    int count = 0;
    while (count < 50) {
        terminal_print_at("Task B: ", 50, 3);
        char buf[12];
        int_to_str(count++, buf);
        terminal_print_colored(buf, VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_print("   ");
        for (volatile int i = 0; i < 700000; i++);
    }
    demo_running--;
}

static void cmd_demo(void) {
    if (!multitasking_enabled()) {
        terminal_print("Multitasking not initialized.\n");
        return;
    }

    terminal_print_colored("Starting multitasking demo...\n", VGA_LIGHT_MAGENTA, VGA_BLACK);
    terminal_print("Two tasks will count concurrently in the top-right.\n\n");

    demo_running = 2;
    int pidA = process_create(demo_task_a, "demo_A");
    int pidB = process_create(demo_task_b, "demo_B");

    if (pidA < 0 || pidB < 0) {
        terminal_print_colored("Failed to create demo tasks.\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    terminal_printf("Created tasks: PID %d (A), PID %d (B)\n", pidA, pidB);
    terminal_print("Shell continues to run concurrently.\n");
}

static void cmd_uname(void) {
    terminal_print_colored("MyOS", VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_print(" v0.2.0 (x86 i386) - built with love and assembly\n");
}

/* ============================================================
 * Shell input / command dispatch
 * ============================================================ */

static void execute_command(void) {
    /* Skip leading spaces */
    char *input = cmd_buf;
    while (*input == ' ') input++;
    if (*input == '\0') return;

    /* Split command and arguments */
    char *cmd = input;
    char *args = NULL;
    for (int i = 0; input[i]; i++) {
        if (input[i] == ' ') {
            input[i] = '\0';
            args = &input[i + 1];
            while (*args == ' ') args++;  /* Skip extra spaces */
            break;
        }
    }

    /* Dispatch commands */
    if (strcmp(cmd, "help") == 0)       cmd_help();
    else if (strcmp(cmd, "clear") == 0) cmd_clear();
    else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
    else if (strcmp(cmd, "meminfo") == 0) cmd_meminfo();
    else if (strcmp(cmd, "echo") == 0)  cmd_echo(args);
    else if (strcmp(cmd, "ls") == 0)    cmd_ls();
    else if (strcmp(cmd, "cat") == 0)   cmd_cat(args);
    else if (strcmp(cmd, "tasks") == 0) cmd_tasks();
    else if (strcmp(cmd, "demo") == 0)  cmd_demo();
    else if (strcmp(cmd, "uname") == 0) cmd_uname();
    else {
        terminal_print_colored("Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        terminal_printf("%s\n", cmd);
        terminal_print("Type 'help' for available commands.\n");
    }
}

static void print_prompt(void) {
    terminal_print_colored("myos", VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_print_colored("> ", VGA_LIGHT_CYAN, VGA_BLACK);
}

void shell_init(void) {
    cmd_len = 0;
    memset(cmd_buf, 0, CMD_BUF_SIZE);
}

void shell_run(void) {
    shell_init();

    terminal_print_colored("\nWelcome to MyOS Shell! Type 'help' for commands.\n\n", VGA_YELLOW, VGA_BLACK);
    print_prompt();

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            terminal_putchar('\n');
            cmd_buf[cmd_len] = '\0';
            execute_command();
            cmd_len = 0;
            memset(cmd_buf, 0, CMD_BUF_SIZE);
            print_prompt();
        } else if (c == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                cmd_buf[cmd_len] = '\0';
                terminal_backspace();
            }
        } else if (c >= ' ' && cmd_len < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_len++] = c;
            terminal_putchar(c);
        }
    }
}
