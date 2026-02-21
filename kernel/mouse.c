#include "mouse.h"
#include "idt.h"
#include "io.h"
#include "fb.h"
#include "event.h"

#define MOUSE_DATA  0x60
#define MOUSE_CMD   0x64
#define MOUSE_STATUS 0x64

static int mouse_x = 400;
static int mouse_y = 300;
static bool btn_left   = false;
static bool btn_right  = false;
static bool btn_middle = false;

static uint8_t mouse_cycle = 0;
static int8_t  mouse_bytes[3];

#define CURSOR_W 12
#define CURSOR_H 19

static const uint8_t cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,1,1,1,1,1},
    {1,2,2,2,1,2,2,1,0,0,0,0},
    {1,2,2,1,0,1,2,2,1,0,0,0},
    {1,2,1,0,0,1,2,2,1,0,0,0},
    {1,1,0,0,0,0,1,2,2,1,0,0},
    {1,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,0,1,2,1,0,0},
    {0,0,0,0,0,0,0,1,1,0,0,0},
};

static uint32_t cursor_save[CURSOR_W * CURSOR_H];
static int cursor_save_x = -1;
static int cursor_save_y = -1;
static bool cursor_visible = false;

static void mouse_wait_write(void) {
    int timeout = 100000;
    while (timeout--) {
        if (!(inb(MOUSE_STATUS) & 0x02)) return;
    }
}

static void mouse_wait_read(void) {
    int timeout = 100000;
    while (timeout--) {
        if (inb(MOUSE_STATUS) & 0x01) return;
    }
}

static void mouse_write(uint8_t val) {
    mouse_wait_write();
    outb(MOUSE_CMD, 0xD4);
    mouse_wait_write();
    outb(MOUSE_DATA, val);
}

static uint8_t mouse_read_data(void) {
    mouse_wait_read();
    return inb(MOUSE_DATA);
}

void mouse_hide_cursor(void) {
    if (!cursor_visible || !fb_is_active()) return;

    uint32_t *fbuf = fb_get_buffer();
    uint32_t pitch = fb_get_pitch();
    uint32_t w = fb_get_width();
    uint32_t h = fb_get_height();

    for (int dy = 0; dy < CURSOR_H; dy++) {
        for (int dx = 0; dx < CURSOR_W; dx++) {
            int px = cursor_save_x + dx;
            int py = cursor_save_y + dy;
            if (px >= 0 && (uint32_t)px < w && py >= 0 && (uint32_t)py < h) {
                if (cursor_bitmap[dy][dx] != 0) {
                    uint32_t *row = (uint32_t *)((uint8_t *)fbuf + py * pitch);
                    row[px] = cursor_save[dy * CURSOR_W + dx];
                }
            }
        }
    }
    cursor_visible = false;
}

void mouse_show_cursor(void) {
    if (!fb_is_active()) return;

    uint32_t *fbuf = fb_get_buffer();
    uint32_t pitch = fb_get_pitch();
    uint32_t w = fb_get_width();
    uint32_t h = fb_get_height();

    cursor_save_x = mouse_x;
    cursor_save_y = mouse_y;

    for (int dy = 0; dy < CURSOR_H; dy++) {
        for (int dx = 0; dx < CURSOR_W; dx++) {
            int px = mouse_x + dx;
            int py = mouse_y + dy;
            if (px >= 0 && (uint32_t)px < w && py >= 0 && (uint32_t)py < h) {
                uint32_t *row = (uint32_t *)((uint8_t *)fbuf + py * pitch);
                cursor_save[dy * CURSOR_W + dx] = row[px];
                if (cursor_bitmap[dy][dx] == 1)
                    row[px] = 0x000000;
                else if (cursor_bitmap[dy][dx] == 2)
                    row[px] = 0xFFFFFF;
            }
        }
    }
    cursor_visible = true;
}

static void mouse_handler(registers_t *regs) {
    (void)regs;
    uint8_t status = inb(MOUSE_STATUS);
    if (!(status & 0x20)) return;

    int8_t data = (int8_t)inb(MOUSE_DATA);

    switch (mouse_cycle) {
        case 0:
            mouse_bytes[0] = data;
            if (data & 0x08) mouse_cycle++;
            break;
        case 1:
            mouse_bytes[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_bytes[2] = data;
            mouse_cycle = 0;

            bool old_left = btn_left;
            btn_left   = mouse_bytes[0] & 0x01;
            btn_right  = mouse_bytes[0] & 0x02;
            btn_middle = mouse_bytes[0] & 0x04;

            int dx = mouse_bytes[1];
            int dy = -mouse_bytes[2];

            if (dx != 0 || dy != 0) {
                mouse_x += dx;
                mouse_y += dy;

                if (mouse_x < 0) mouse_x = 0;
                if (mouse_y < 0) mouse_y = 0;
                if (fb_is_active()) {
                    if (mouse_x >= (int)fb_get_width()) mouse_x = fb_get_width() - 1;
                    if (mouse_y >= (int)fb_get_height()) mouse_y = fb_get_height() - 1;
                }

                event_push_mouse_move(mouse_x, mouse_y);
            }

            if (btn_left != old_left) {
                event_push_mouse_button(0, btn_left);
            }
            if ((mouse_bytes[0] & 0x02) != 0) {
            }
            break;
    }
}

void mouse_init(void) {
    mouse_wait_write();
    outb(MOUSE_CMD, 0xA8);

    mouse_wait_write();
    outb(MOUSE_CMD, 0x20);
    mouse_wait_read();
    uint8_t status = inb(MOUSE_DATA);
    status |= 0x02;
    status &= ~0x20;
    mouse_wait_write();
    outb(MOUSE_CMD, 0x60);
    mouse_wait_write();
    outb(MOUSE_DATA, status);

    mouse_write(0xFF);
    mouse_read_data();
    mouse_read_data();
    mouse_read_data();

    mouse_write(0xF6);
    mouse_read_data();

    mouse_write(0xF4);
    mouse_read_data();

    uint8_t mask = inb(0xA1);
    mask &= ~(1 << 4);
    outb(0xA1, mask);

    irq_install_handler(12, mouse_handler);
}

int  mouse_get_x(void)       { return mouse_x; }
int  mouse_get_y(void)       { return mouse_y; }
bool mouse_button_left(void)  { return btn_left; }
bool mouse_button_right(void) { return btn_right; }
bool mouse_button_middle(void){ return btn_middle; }
