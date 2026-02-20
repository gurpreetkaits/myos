#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

void keyboard_init(void);
char keyboard_getchar(void);    /* blocking: waits for a keypress */
bool keyboard_has_input(void);  /* check if key available */
char keyboard_read(void);       /* non-blocking: returns 0 if no key */

#endif
