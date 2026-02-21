#include "vga.h"
#include "types.h"
#include "string.h"
#include "io.h"
#include "fb.h"
#include "font.h"

static unsigned short *vga_buffer = (unsigned short *)VGA_MEMORY;
static int cursor_col = 0;
static int cursor_row = 0;
static unsigned char terminal_color = 0x0F;

static int term_cols = VGA_COLS;
static int term_rows = VGA_ROWS;
static bool use_fb = false;

static const uint32_t vga_to_rgb[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

static unsigned short vga_entry(char c, unsigned char color) {
    return (unsigned short)c | (unsigned short)color << 8;
}

static void update_cursor(void) {
    if (use_fb) return;
    uint16_t pos = cursor_row * VGA_COLS + cursor_col;
    outb(0x3D4, 14);
    outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

static void fb_draw_grid_char(int col, int row, char c, unsigned char color) {
    uint32_t fg = vga_to_rgb[color & 0x0F];
    uint32_t bg = vga_to_rgb[(color >> 4) & 0x0F];
    fb_draw_char(col * 8, row * 16, c, fg, bg);
}

void terminal_init(void) {
    terminal_color = (VGA_BLACK << 4) | VGA_WHITE;

    if (fb_is_active()) {
        use_fb = true;
        term_cols = fb_get_width() / 8;
        term_rows = fb_get_height() / 16;
        fb_clear(0x000000);
    } else {
        use_fb = false;
        term_cols = VGA_COLS;
        term_rows = VGA_ROWS;
        outb(0x3D4, 0x0A);
        outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
        outb(0x3D4, 0x0B);
        outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
    }

    terminal_clear();
}

void terminal_clear(void) {
    if (use_fb) {
        fb_clear(vga_to_rgb[(terminal_color >> 4) & 0x0F]);
    } else {
        for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) {
            vga_buffer[i] = vga_entry(' ', terminal_color);
        }
    }
    cursor_col = 0;
    cursor_row = 0;
    update_cursor();
}

void terminal_setcolor(unsigned char fg, unsigned char bg) {
    terminal_color = (bg << 4) | fg;
}

static void terminal_scroll(void) {
    if (use_fb) {
        uint32_t *fbuf = fb_get_buffer();
        uint32_t pitch = fb_get_pitch();
        uint32_t h = fb_get_height();
        uint32_t w = fb_get_width();
        for (uint32_t y = 0; y < h - 16; y++) {
            uint32_t *dst = (uint32_t *)((uint8_t *)fbuf + y * pitch);
            uint32_t *src = (uint32_t *)((uint8_t *)fbuf + (y + 16) * pitch);
            memcpy(dst, src, w * 4);
        }
        uint32_t bg = vga_to_rgb[(terminal_color >> 4) & 0x0F];
        for (uint32_t y = h - 16; y < h; y++) {
            uint32_t *dst = (uint32_t *)((uint8_t *)fbuf + y * pitch);
            for (uint32_t x = 0; x < w; x++) dst[x] = bg;
        }
    } else {
        for (int i = 0; i < (VGA_ROWS - 1) * VGA_COLS; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_COLS];
        }
        for (int i = (VGA_ROWS - 1) * VGA_COLS; i < VGA_ROWS * VGA_COLS; i++) {
            vga_buffer[i] = vga_entry(' ', terminal_color);
        }
    }
    cursor_row = term_rows - 1;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 4) & ~3;
    } else if (c == '\b') {
        terminal_backspace();
        return;
    } else {
        if (use_fb) {
            fb_draw_grid_char(cursor_col, cursor_row, c, terminal_color);
        } else {
            int offset = cursor_row * VGA_COLS + cursor_col;
            vga_buffer[offset] = vga_entry(c, terminal_color);
        }
        cursor_col++;
    }

    if (cursor_col >= term_cols) {
        cursor_col = 0;
        cursor_row++;
    }
    if (cursor_row >= term_rows) {
        terminal_scroll();
    }
    update_cursor();
}

void terminal_print(const char *str) {
    while (*str) {
        terminal_putchar(*str++);
    }
}

void terminal_print_at(const char *str, int col, int row) {
    cursor_col = col;
    cursor_row = row;
    terminal_print(str);
    update_cursor();
}

void terminal_print_colored(const char *str, unsigned char fg, unsigned char bg) {
    unsigned char old_color = terminal_color;
    terminal_setcolor(fg, bg);
    terminal_print(str);
    terminal_color = old_color;
}

void terminal_backspace(void) {
    if (cursor_col > 0) {
        cursor_col--;
    } else if (cursor_row > 0) {
        cursor_row--;
        cursor_col = term_cols - 1;
    }
    if (use_fb) {
        fb_draw_grid_char(cursor_col, cursor_row, ' ', terminal_color);
    } else {
        int offset = cursor_row * VGA_COLS + cursor_col;
        vga_buffer[offset] = vga_entry(' ', terminal_color);
    }
    update_cursor();
}

int terminal_get_col(void) { return cursor_col; }
int terminal_get_row(void) { return cursor_row; }

void terminal_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buf[16];

    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    terminal_print(s ? s : "(null)");
                    break;
                }
                case 'd': {
                    int val = va_arg(args, int);
                    int_to_str(val, buf);
                    terminal_print(buf);
                    break;
                }
                case 'x': {
                    uint32_t val = va_arg(args, uint32_t);
                    uint_to_hex(val, buf);
                    terminal_print(buf);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    terminal_putchar(c);
                    break;
                }
                case '%':
                    terminal_putchar('%');
                    break;
                default:
                    terminal_putchar('%');
                    terminal_putchar(*fmt);
                    break;
            }
        } else {
            terminal_putchar(*fmt);
        }
        fmt++;
    }

    va_end(args);
}
