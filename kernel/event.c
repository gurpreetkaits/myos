#include "event.h"
#include "string.h"

#define EVENT_QUEUE_SIZE 256

static event_t event_queue[EVENT_QUEUE_SIZE];
static volatile int eq_head = 0;
static volatile int eq_tail = 0;

void event_init(void) {
    eq_head = 0;
    eq_tail = 0;
    memset(event_queue, 0, sizeof(event_queue));
}

void event_push(event_t *e) {
    int next = (eq_head + 1) % EVENT_QUEUE_SIZE;
    if (next == eq_tail) return;
    event_queue[eq_head] = *e;
    eq_head = next;
}

bool event_poll(event_t *e) {
    if (eq_head == eq_tail) return false;
    *e = event_queue[eq_tail];
    eq_tail = (eq_tail + 1) % EVENT_QUEUE_SIZE;
    return true;
}

void event_push_key(char key) {
    event_t e;
    e.type = EVENT_KEY_PRESS;
    e.key.key = key;
    event_push(&e);
}

void event_push_mouse_move(int x, int y) {
    event_t e;
    e.type = EVENT_MOUSE_MOVE;
    e.mouse_move.x = x;
    e.mouse_move.y = y;
    event_push(&e);
}

void event_push_mouse_button(int button, bool pressed) {
    event_t e;
    e.type = EVENT_MOUSE_BUTTON;
    e.mouse_button.button = button;
    e.mouse_button.pressed = pressed;
    event_push(&e);
}

void event_push_timer(uint32_t ticks) {
    event_t e;
    e.type = EVENT_TIMER;
    e.timer.ticks = ticks;
    event_push(&e);
}
