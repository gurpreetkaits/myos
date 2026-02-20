#include "keyboard.h"
#include "idt.h"
#include "io.h"
#include "vga.h"

#define KB_DATA_PORT 0x60
#define KB_BUFFER_SIZE 128

/* Circular buffer for keypresses */
static char kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_head = 0;
static volatile int kb_tail = 0;

/* Modifier key state */
static bool shift_held = false;
static bool caps_lock  = false;

/* Scancode set 1 → ASCII (US QWERTY layout) */
static const char scancode_to_ascii[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,  ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* F1-F10 */
    0, 0,                            /* Num/Scroll lock */
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0,
    0, 0, 0,                         /* F11, F12 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Shifted characters */
static const char scancode_to_ascii_shift[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0,  ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0,
    0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void kb_buffer_push(char c) {
    int next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {
        kb_buffer[kb_head] = c;
        kb_head = next;
    }
}

static void keyboard_handler(registers_t *regs) {
    (void)regs;
    uint8_t scancode = inb(KB_DATA_PORT);

    /* Key release (bit 7 set) */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) shift_held = false;  /* Shift released */
        return;
    }

    /* Modifier keys */
    if (scancode == 0x2A || scancode == 0x36) { shift_held = true; return; }
    if (scancode == 0x3A) { caps_lock = !caps_lock; return; }  /* Caps Lock toggle */

    /* Translate scancode to ASCII */
    char c;
    if (shift_held) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }

    /* Apply caps lock to letters */
    if (caps_lock && c >= 'a' && c <= 'z') c -= 32;
    else if (caps_lock && c >= 'A' && c <= 'Z') c += 32;

    if (c != 0) {
        kb_buffer_push(c);
    }
}

void keyboard_init(void) {
    irq_install_handler(1, keyboard_handler);
}

bool keyboard_has_input(void) {
    return kb_head != kb_tail;
}

char keyboard_read(void) {
    if (kb_head == kb_tail) return 0;
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

char keyboard_getchar(void) {
    while (!keyboard_has_input()) {
        hlt();  /* Sleep until next interrupt */
    }
    return keyboard_read();
}
