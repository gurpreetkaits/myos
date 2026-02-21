#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

void mouse_init(void);
int  mouse_get_x(void);
int  mouse_get_y(void);
bool mouse_button_left(void);
bool mouse_button_right(void);
bool mouse_button_middle(void);

void mouse_hide_cursor(void);
void mouse_show_cursor(void);

#endif
