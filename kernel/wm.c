#include "wm.h"
#include "fb.h"
#include "font.h"
#include "mouse.h"
#include "event.h"
#include "memory.h"
#include "string.h"
#include "idt.h"
#include "process.h"
#include "io.h"

static uint32_t *backbuf = NULL;
static uint32_t screen_w, screen_h, screen_pitch;

static window_t windows[WM_MAX_WINDOWS];
static int zorder[WM_MAX_WINDOWS];
static int num_windows = 0;

static bool dragging = false;
static int  drag_win = -1;
static int  drag_off_x, drag_off_y;

static uint32_t last_tick = 0;


static void bb_putpixel(int x, int y, uint32_t color) {
    if (x >= 0 && (uint32_t)x < screen_w && y >= 0 && (uint32_t)y < screen_h) {
        uint32_t *row = (uint32_t *)((uint8_t *)backbuf + y * screen_pitch);
        row[x] = color;
    }
}

static void bb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int row = y; row < y + h; row++) {
        if (row < 0 || (uint32_t)row >= screen_h) continue;
        uint32_t *line = (uint32_t *)((uint8_t *)backbuf + row * screen_pitch);
        int x0 = x < 0 ? 0 : x;
        int x1 = x + w;
        if ((uint32_t)x1 > screen_w) x1 = screen_w;
        for (int col = x0; col < x1; col++) {
            line[col] = color;
        }
    }
}

static void bb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = font_get_data() + (uint8_t)c * 16;
    for (int row = 0; row < 16; row++) {
        if (y + row < 0 || (uint32_t)(y + row) >= screen_h) continue;
        uint32_t *line = (uint32_t *)((uint8_t *)backbuf + (y + row) * screen_pitch);
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px >= 0 && (uint32_t)px < screen_w) {
                line[px] = (bits & (0x80 >> col)) ? fg : bg;
            }
        }
    }
}

static void bb_draw_char_nobg(int x, int y, char c, uint32_t fg) {
    const uint8_t *glyph = font_get_data() + (uint8_t)c * 16;
    for (int row = 0; row < 16; row++) {
        if (y + row < 0 || (uint32_t)(y + row) >= screen_h) continue;
        uint32_t *line = (uint32_t *)((uint8_t *)backbuf + (y + row) * screen_pitch);
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px >= 0 && (uint32_t)px < screen_w && (bits & (0x80 >> col))) {
                line[px] = fg;
            }
        }
    }
}

static void bb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        bb_draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

static void bb_draw_string_nobg(int x, int y, const char *str, uint32_t fg) {
    while (*str) {
        bb_draw_char_nobg(x, y, *str, fg);
        x += 8;
        str++;
    }
}

static void bb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = x; i < x + w; i++) {
        bb_putpixel(i, y, color);
        bb_putpixel(i, y + h - 1, color);
    }
    for (int i = y; i < y + h; i++) {
        bb_putpixel(x, i, color);
        bb_putpixel(x + w - 1, i, color);
    }
}


void wm_init(void) {
    screen_w = fb_get_width();
    screen_h = fb_get_height();
    screen_pitch = fb_get_pitch();

    uint32_t bb_size = screen_pitch * screen_h;
    uint32_t pages_needed = (bb_size + 4095) / 4096;
    backbuf = (uint32_t *)pmm_alloc_page();
    if (!backbuf) return;
    for (uint32_t i = 1; i < pages_needed; i++) {
        pmm_alloc_page();
    }

    memset(backbuf, 0, bb_size);
    memset(windows, 0, sizeof(windows));
    memset(zorder, 0, sizeof(zorder));
    num_windows = 0;

    int about_id = wm_create_window("About MyOS", 50, 50, 300, 200);
    if (about_id >= 0) {
        wm_fill_window(about_id, FB_WINDOW_BG);
        wm_draw_string_to_window(about_id, 20, 20, "MyOS v0.3.0", 0x000000, FB_WINDOW_BG);
        wm_draw_string_to_window(about_id, 20, 44, "A hobby OS built", 0x444444, FB_WINDOW_BG);
        wm_draw_string_to_window(about_id, 20, 60, "from scratch in", 0x444444, FB_WINDOW_BG);
        wm_draw_string_to_window(about_id, 20, 76, "C and x86 assembly.", 0x444444, FB_WINDOW_BG);
        wm_draw_string_to_window(about_id, 20, 108, "Features:", 0x000000, FB_WINDOW_BG);
        wm_draw_string_to_window(about_id, 20, 124, "- VESA graphics", 0x2060A0, FB_WINDOW_BG);
        wm_draw_string_to_window(about_id, 20, 140, "- Window manager", 0x2060A0, FB_WINDOW_BG);
        wm_draw_string_to_window(about_id, 20, 156, "- Multitasking", 0x2060A0, FB_WINDOW_BG);
    }

    int sys_id = wm_create_window("System Info", 400, 80, 350, 250);
    if (sys_id >= 0) {
        windows[sys_id].dirty = true;
    }
}

int wm_create_window(const char *title, int x, int y, int w, int h) {
    if (num_windows >= WM_MAX_WINDOWS) return -1;

    int id = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!windows[i].visible) { id = i; break; }
    }
    if (id < 0) return -1;

    int cw = w - 2 * WM_BORDER_W;
    int ch = h - WM_TITLEBAR_H - WM_BORDER_W;

    windows[id].x = x;
    windows[id].y = y;
    windows[id].width = w;
    windows[id].height = h;
    windows[id].content_w = cw;
    windows[id].content_h = ch;
    windows[id].visible = true;
    windows[id].focused = true;
    windows[id].dirty = true;

    strncpy(windows[id].title, title, 31);
    windows[id].title[31] = '\0';

    uint32_t buf_size = cw * ch * 4;
    windows[id].content = (uint32_t *)kmalloc(buf_size);
    if (windows[id].content) {
        for (int i = 0; i < cw * ch; i++) {
            windows[id].content[i] = FB_WINDOW_BG;
        }
    }

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (i != id && windows[i].visible) windows[i].focused = false;
    }

    zorder[num_windows] = id;
    num_windows++;

    return id;
}

void wm_destroy_window(int id) {
    if (id < 0 || id >= WM_MAX_WINDOWS || !windows[id].visible) return;

    windows[id].visible = false;
    if (windows[id].content) {
        kfree(windows[id].content);
        windows[id].content = NULL;
    }

    int pos = -1;
    for (int i = 0; i < num_windows; i++) {
        if (zorder[i] == id) { pos = i; break; }
    }
    if (pos >= 0) {
        for (int i = pos; i < num_windows - 1; i++) {
            zorder[i] = zorder[i + 1];
        }
        num_windows--;
    }

    if (num_windows > 0) {
        windows[zorder[num_windows - 1]].focused = true;
    }
}

void wm_fill_window(int id, uint32_t color) {
    if (id < 0 || id >= WM_MAX_WINDOWS || !windows[id].content) return;
    int total = windows[id].content_w * windows[id].content_h;
    for (int i = 0; i < total; i++) {
        windows[id].content[i] = color;
    }
}

void wm_draw_to_window(int id, uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if (id < 0 || id >= WM_MAX_WINDOWS || !windows[id].content) return;
    const uint8_t *glyph = font_get_data() + (uint8_t)c * 16;
    int cw = windows[id].content_w;
    int ch = windows[id].content_h;

    for (int row = 0; row < 16; row++) {
        if ((int)(y + row) >= ch) break;
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if ((int)(x + col) >= cw) break;
            int idx = (y + row) * cw + (x + col);
            windows[id].content[idx] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void wm_draw_string_to_window(int id, uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        wm_draw_to_window(id, x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}


static void draw_desktop(void) {
    for (uint32_t y = 0; y < screen_h - WM_TASKBAR_H; y++) {
        uint32_t *line = (uint32_t *)((uint8_t *)backbuf + y * screen_pitch);
        uint8_t r = 20 + (y * 30) / screen_h;
        uint8_t g = 40 + (y * 40) / screen_h;
        uint8_t b = 100 + (y * 60) / screen_h;
        uint32_t color = ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16);
        for (uint32_t x = 0; x < screen_w; x++) {
            line[x] = color;
        }
    }
}

static void draw_window(int id) {
    window_t *w = &windows[id];
    if (!w->visible) return;

    int wx = w->x, wy = w->y;
    int ww = w->width, wh = w->height;

    bb_fill_rect(wx + 3, wy + 3, ww, wh, 0x00000040);
    bb_fill_rect(wx, wy, ww, wh, FB_BORDER);

    uint32_t tb_color = w->focused ? FB_TITLEBAR_ACTIVE : FB_TITLEBAR;
    bb_fill_rect(wx + 1, wy + 1, ww - 2, WM_TITLEBAR_H - 1, tb_color);
    bb_draw_string_nobg(wx + 8, wy + 3, w->title, FB_WHITE);

    int cx = wx + ww - WM_CLOSE_SIZE - 4;
    int cy = wy + 3;
    bb_fill_rect(cx, cy, WM_CLOSE_SIZE, WM_CLOSE_SIZE, FB_CLOSE_RED);
    for (int i = 3; i < WM_CLOSE_SIZE - 3; i++) {
        bb_putpixel(cx + i, cy + i, FB_WHITE);
        bb_putpixel(cx + WM_CLOSE_SIZE - 1 - i, cy + i, FB_WHITE);
        bb_putpixel(cx + i + 1, cy + i, FB_WHITE);
        bb_putpixel(cx + WM_CLOSE_SIZE - i, cy + i, FB_WHITE);
    }

    int content_x = wx + WM_BORDER_W;
    int content_y = wy + WM_TITLEBAR_H;

    if (w->content) {
        for (int row = 0; row < w->content_h; row++) {
            int sy = content_y + row;
            if (sy < 0 || (uint32_t)sy >= screen_h) continue;
            uint32_t *dst_line = (uint32_t *)((uint8_t *)backbuf + sy * screen_pitch);
            uint32_t *src_line = w->content + row * w->content_w;
            for (int col = 0; col < w->content_w; col++) {
                int sx = content_x + col;
                if (sx >= 0 && (uint32_t)sx < screen_w) {
                    dst_line[sx] = src_line[col];
                }
            }
        }
    } else {
        bb_fill_rect(content_x, content_y, w->content_w, w->content_h, FB_WINDOW_BG);
    }
}

static void update_system_info_window(void) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].visible && strcmp(windows[i].title, "System Info") == 0) {
            wm_fill_window(i, FB_WINDOW_BG);

            char buf[64];

            wm_draw_string_to_window(i, 16, 12, "System Information", 0x000000, FB_WINDOW_BG);

            uint32_t ticks = timer_get_ticks();
            uint32_t secs = ticks / 100;
            uint32_t mins = secs / 60;
            secs %= 60;

            strcpy(buf, "Uptime: ");
            char num[12];
            int_to_str(mins, num);
            strcat(buf, num);
            strcat(buf, "m ");
            int_to_str(secs, num);
            strcat(buf, num);
            strcat(buf, "s");
            wm_draw_string_to_window(i, 16, 40, buf, 0x444444, FB_WINDOW_BG);

            strcpy(buf, "Free RAM: ");
            int_to_str(pmm_get_free_pages() * 4, num);
            strcat(buf, num);
            strcat(buf, " KB");
            wm_draw_string_to_window(i, 16, 64, buf, 0x444444, FB_WINDOW_BG);

            strcpy(buf, "Heap used: ");
            int_to_str(heap_get_used() / 1024, num);
            strcat(buf, num);
            strcat(buf, " KB");
            wm_draw_string_to_window(i, 16, 88, buf, 0x444444, FB_WINDOW_BG);

            strcpy(buf, "Processes: ");
            int_to_str(process_count(), num);
            strcat(buf, num);
            wm_draw_string_to_window(i, 16, 112, buf, 0x444444, FB_WINDOW_BG);

            strcpy(buf, "Display: ");
            int_to_str(screen_w, num);
            strcat(buf, num);
            strcat(buf, "x");
            int_to_str(screen_h, num);
            strcat(buf, num);
            strcat(buf, " 32bpp");
            wm_draw_string_to_window(i, 16, 136, buf, 0x444444, FB_WINDOW_BG);

            strcpy(buf, "Mouse: ");
            int_to_str(mouse_get_x(), num);
            strcat(buf, num);
            strcat(buf, ", ");
            int_to_str(mouse_get_y(), num);
            strcat(buf, num);
            wm_draw_string_to_window(i, 16, 160, buf, 0x444444, FB_WINDOW_BG);

            strcpy(buf, "Windows: ");
            int_to_str(num_windows, num);
            strcat(buf, num);
            wm_draw_string_to_window(i, 16, 184, buf, 0x444444, FB_WINDOW_BG);

            break;
        }
    }
}

static void draw_taskbar(void) {
    int tb_y = screen_h - WM_TASKBAR_H;

    bb_fill_rect(0, tb_y, screen_w, WM_TASKBAR_H, FB_TASKBAR);
    for (uint32_t x = 0; x < screen_w; x++) {
        bb_putpixel(x, tb_y, 0x555555);
    }

    int btn_x = 4;
    for (int i = 0; i < num_windows; i++) {
        int wid = zorder[i];
        if (!windows[wid].visible) continue;

        uint32_t btn_color = windows[wid].focused ? 0x4488CC : 0x555555;
        bb_fill_rect(btn_x, tb_y + 4, 120, 22, btn_color);
        bb_draw_string_nobg(btn_x + 4, tb_y + 7, windows[wid].title, FB_WHITE);
        btn_x += 124;
    }

    uint32_t ticks = timer_get_ticks();
    uint32_t secs = ticks / 100;
    uint32_t mins = secs / 60;
    uint32_t hrs = mins / 60;
    mins %= 60;
    secs %= 60;

    char clock[16];
    char num[4];

    int_to_str(hrs, num);
    if (hrs < 10) { clock[0] = '0'; strcpy(clock + 1, num); } else { strcpy(clock, num); }
    int len = strlen(clock);
    clock[len] = ':';
    int_to_str(mins, num);
    if (mins < 10) { clock[len+1] = '0'; strcpy(clock + len + 2, num); } else { strcpy(clock + len + 1, num); }
    len = strlen(clock);
    clock[len] = ':';
    int_to_str(secs, num);
    if (secs < 10) { clock[len+1] = '0'; strcpy(clock + len + 2, num); } else { strcpy(clock + len + 1, num); }

    bb_draw_string_nobg(screen_w - strlen(clock) * 8 - 8, tb_y + 7, clock, FB_LIGHT_GREY);
}

static void draw_cursor(void) {
    int mx = mouse_get_x();
    int my = mouse_get_y();

    static const uint8_t cursor_bmp[19][12] = {
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

    for (int dy = 0; dy < 19; dy++) {
        for (int dx = 0; dx < 12; dx++) {
            if (cursor_bmp[dy][dx] == 1)
                bb_putpixel(mx + dx, my + dy, 0x000000);
            else if (cursor_bmp[dy][dx] == 2)
                bb_putpixel(mx + dx, my + dy, 0xFFFFFF);
        }
    }
}

static void wm_compose(void) {
    draw_desktop();

    for (int i = 0; i < num_windows; i++) {
        draw_window(zorder[i]);
    }

    draw_taskbar();
    draw_cursor();

    fb_blit_full(backbuf);
}


static int hit_test_window(int mx, int my) {
    for (int i = num_windows - 1; i >= 0; i--) {
        int wid = zorder[i];
        window_t *w = &windows[wid];
        if (!w->visible) continue;
        if (mx >= w->x && mx < w->x + w->width &&
            my >= w->y && my < w->y + w->height) {
            return wid;
        }
    }
    return -1;
}

static int hit_test_taskbar(int mx, int my) {
    int tb_y = screen_h - WM_TASKBAR_H;
    if (my < (int)tb_y || my >= (int)screen_h) return -1;

    int btn_x = 4;
    for (int i = 0; i < num_windows; i++) {
        int wid = zorder[i];
        if (!windows[wid].visible) continue;
        if (mx >= btn_x && mx < btn_x + 120) return wid;
        btn_x += 124;
    }
    return -1;
}

static bool hit_test_close(int wid, int mx, int my) {
    window_t *w = &windows[wid];
    int cx = w->x + w->width - WM_CLOSE_SIZE - 4;
    int cy = w->y + 3;
    return mx >= cx && mx < cx + WM_CLOSE_SIZE &&
           my >= cy && my < cy + WM_CLOSE_SIZE;
}

static bool hit_test_titlebar(int wid, int mx, int my) {
    window_t *w = &windows[wid];
    return mx >= w->x && mx < w->x + w->width &&
           my >= w->y && my < w->y + WM_TITLEBAR_H;
}

static void raise_window(int wid) {
    int pos = -1;
    for (int i = 0; i < num_windows; i++) {
        if (zorder[i] == wid) { pos = i; break; }
    }
    if (pos < 0 || pos == num_windows - 1) return;

    for (int i = pos; i < num_windows - 1; i++) {
        zorder[i] = zorder[i + 1];
    }
    zorder[num_windows - 1] = wid;
}

static void focus_window(int wid) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i].focused = (i == wid);
    }
    raise_window(wid);
}

static void handle_mouse_down(int mx, int my) {
    int tb_win = hit_test_taskbar(mx, my);
    if (tb_win >= 0) {
        focus_window(tb_win);
        return;
    }

    int wid = hit_test_window(mx, my);
    if (wid < 0) return;

    focus_window(wid);

    if (hit_test_close(wid, mx, my)) {
        wm_destroy_window(wid);
        return;
    }

    if (hit_test_titlebar(wid, mx, my)) {
        dragging = true;
        drag_win = wid;
        drag_off_x = mx - windows[wid].x;
        drag_off_y = my - windows[wid].y;
    }
}

static void handle_mouse_up(void) {
    dragging = false;
    drag_win = -1;
}

static void handle_mouse_move(int mx, int my) {
    if (dragging && drag_win >= 0 && windows[drag_win].visible) {
        windows[drag_win].x = mx - drag_off_x;
        windows[drag_win].y = my - drag_off_y;

        if (windows[drag_win].x < -windows[drag_win].width + 40)
            windows[drag_win].x = -windows[drag_win].width + 40;
        if (windows[drag_win].y < 0) windows[drag_win].y = 0;
        if (windows[drag_win].x > (int)screen_w - 40)
            windows[drag_win].x = screen_w - 40;
        if (windows[drag_win].y > (int)screen_h - WM_TASKBAR_H - 20)
            windows[drag_win].y = screen_h - WM_TASKBAR_H - 20;
    }
}


void wm_run(void) {
    while (1) {
        event_t e;
        while (event_poll(&e)) {
            switch (e.type) {
                case EVENT_MOUSE_BUTTON:
                    if (e.mouse_button.pressed)
                        handle_mouse_down(mouse_get_x(), mouse_get_y());
                    else
                        handle_mouse_up();
                    break;
                case EVENT_MOUSE_MOVE:
                    handle_mouse_move(e.mouse_move.x, e.mouse_move.y);
                    break;
                case EVENT_KEY_PRESS:
                    break;
                default:
                    break;
            }
        }

        uint32_t now = timer_get_ticks();
        if (now - last_tick >= 50) {
            last_tick = now;
            update_system_info_window();
        }

        wm_compose();
        hlt();
    }
}
