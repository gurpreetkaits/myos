#ifndef FB_H
#define FB_H

#include "types.h"

#define FB_RGB(r, g, b) ((uint32_t)(b) | ((uint32_t)(g) << 8) | ((uint32_t)(r) << 16))

#define FB_BLACK       FB_RGB(0, 0, 0)
#define FB_WHITE       FB_RGB(255, 255, 255)
#define FB_RED         FB_RGB(200, 50, 50)
#define FB_GREEN       FB_RGB(50, 200, 50)
#define FB_BLUE        FB_RGB(50, 80, 180)
#define FB_YELLOW      FB_RGB(220, 200, 50)
#define FB_CYAN        FB_RGB(50, 200, 200)
#define FB_MAGENTA     FB_RGB(200, 50, 200)
#define FB_DARK_GREY   FB_RGB(60, 60, 60)
#define FB_LIGHT_GREY  FB_RGB(180, 180, 180)
#define FB_DESKTOP_BG  FB_RGB(30, 60, 120)
#define FB_TITLEBAR    FB_RGB(60, 60, 60)
#define FB_TITLEBAR_ACTIVE FB_RGB(40, 100, 180)
#define FB_TASKBAR     FB_RGB(35, 35, 40)
#define FB_WINDOW_BG   FB_RGB(240, 240, 240)
#define FB_BORDER      FB_RGB(100, 100, 100)
#define FB_CLOSE_RED   FB_RGB(220, 60, 60)

bool fb_init(void);
bool fb_is_active(void);
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pitch(void);
uint32_t *fb_get_buffer(void);

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void fb_clear(uint32_t color);

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);

void fb_blit(uint32_t *src, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t src_pitch);
void fb_blit_full(uint32_t *src);

#endif
