#ifndef EVENT_H
#define EVENT_H

#include "types.h"

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEY_PRESS,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_BUTTON,
    EVENT_TIMER
} event_type_t;

typedef struct {
    event_type_t type;
    union {
        struct { char key; } key;
        struct { int x, y; } mouse_move;
        struct { int button; bool pressed; } mouse_button;
        struct { uint32_t ticks; } timer;
    };
} event_t;

void event_init(void);
void event_push(event_t *e);
bool event_poll(event_t *e);

void event_push_key(char key);
void event_push_mouse_move(int x, int y);
void event_push_mouse_button(int button, bool pressed);
void event_push_timer(uint32_t ticks);

#endif
