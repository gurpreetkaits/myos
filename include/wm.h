#ifndef WM_H
#define WM_H

#include "types.h"

#define WM_MAX_WINDOWS  16
#define WM_TITLEBAR_H   22
#define WM_TASKBAR_H    30
#define WM_BORDER_W     1
#define WM_CLOSE_SIZE   16

typedef struct {
    int x, y;
    int width, height;
    char title[32];
    uint32_t *content;
    int content_w, content_h;
    bool visible;
    bool focused;
    bool dirty;
} window_t;

void wm_init(void);
void wm_run(void);   /* Main event loop (never returns) */
int  wm_create_window(const char *title, int x, int y, int w, int h);
void wm_destroy_window(int id);
void wm_draw_to_window(int id, uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void wm_draw_string_to_window(int id, uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);
void wm_fill_window(int id, uint32_t color);

#endif
