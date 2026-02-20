#ifndef VGA_H
#define VGA_H

enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15,
};

#define VGA_COLS 80
#define VGA_ROWS 25
#define VGA_MEMORY 0xB8000

void terminal_init(void);
void terminal_clear(void);
void terminal_setcolor(unsigned char fg, unsigned char bg);
void terminal_putchar(char c);
void terminal_print(const char *str);
void terminal_print_at(const char *str, int col, int row);
void terminal_print_colored(const char *str, unsigned char fg, unsigned char bg);
void terminal_backspace(void);
void terminal_printf(const char *fmt, ...);
int  terminal_get_col(void);
int  terminal_get_row(void);

#endif
