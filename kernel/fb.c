#include "fb.h"
#include "bootinfo.h"
#include "memory.h"
#include "font.h"
#include "string.h"

static uint32_t *fb_ptr = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static bool fb_active = false;

bool fb_init(void) {
    bootinfo_t *bi = bootinfo_get();
    if (!bi || !bi->vesa_mode) return false;

    fb_width  = bi->fb_width;
    fb_height = bi->fb_height;
    fb_pitch  = bi->fb_pitch;

    uint32_t fb_phys = bi->fb_addr;
    uint32_t fb_size = fb_pitch * fb_height;
    fb_size = (fb_size + 4095) & ~4095;

    paging_map_region(fb_phys, fb_phys, fb_size, 0x03);

    fb_ptr = (uint32_t *)fb_phys;
    fb_active = true;

    return true;
}

bool fb_is_active(void) {
    return fb_active;
}

uint32_t fb_get_width(void)  { return fb_width; }
uint32_t fb_get_height(void) { return fb_height; }
uint32_t fb_get_pitch(void)  { return fb_pitch; }
uint32_t *fb_get_buffer(void) { return fb_ptr; }

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x < fb_width && y < fb_height) {
        uint32_t *row = (uint32_t *)((uint8_t *)fb_ptr + y * fb_pitch);
        row[x] = color;
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t row = y; row < y + h && row < fb_height; row++) {
        uint32_t *line = (uint32_t *)((uint8_t *)fb_ptr + row * fb_pitch);
        uint32_t x_end = x + w;
        if (x_end > fb_width) x_end = fb_width;
        for (uint32_t col = x; col < x_end; col++) {
            line[col] = color;
        }
    }
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = x; i < x + w && i < fb_width; i++) {
        fb_putpixel(i, y, color);
        if (y + h - 1 < fb_height)
            fb_putpixel(i, y + h - 1, color);
    }
    for (uint32_t i = y; i < y + h && i < fb_height; i++) {
        fb_putpixel(x, i, color);
        if (x + w - 1 < fb_width)
            fb_putpixel(x + w - 1, i, color);
    }
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int err = dx - dy;

    while (1) {
        fb_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void fb_clear(uint32_t color) {
    for (uint32_t y = 0; y < fb_height; y++) {
        uint32_t *line = (uint32_t *)((uint8_t *)fb_ptr + y * fb_pitch);
        for (uint32_t x = 0; x < fb_width; x++) {
            line[x] = color;
        }
    }
}

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = font_get_data() + (uint8_t)c * 16;

    for (int row = 0; row < 16; row++) {
        uint32_t *line = (uint32_t *)((uint8_t *)fb_ptr + (y + row) * fb_pitch);
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (x + col < fb_width && y + row < fb_height) {
                line[x + col] = (bits & (0x80 >> col)) ? fg : bg;
            }
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        if (*str == '\n') {
            y += 16;
            x = 0;
        } else {
            fb_draw_char(x, y, *str, fg, bg);
            x += 8;
        }
        str++;
    }
}

void fb_blit(uint32_t *src, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t src_pitch) {
    for (uint32_t row = 0; row < h && y + row < fb_height; row++) {
        uint32_t *dst_line = (uint32_t *)((uint8_t *)fb_ptr + (y + row) * fb_pitch);
        uint32_t *src_line = (uint32_t *)((uint8_t *)src + row * src_pitch);
        uint32_t cols = w;
        if (x + cols > fb_width) cols = fb_width - x;
        memcpy(&dst_line[x], src_line, cols * 4);
    }
}

void fb_blit_full(uint32_t *src) {
    for (uint32_t row = 0; row < fb_height; row++) {
        uint32_t *dst_line = (uint32_t *)((uint8_t *)fb_ptr + row * fb_pitch);
        uint32_t *src_line = (uint32_t *)((uint8_t *)src + row * fb_pitch);
        memcpy(dst_line, src_line, fb_width * 4);
    }
}
