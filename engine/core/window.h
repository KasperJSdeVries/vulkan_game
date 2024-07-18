#ifndef WINDOW_H
#define WINDOW_H

#include "engine.h"

typedef struct window_create_info {
    u32 width, height;
} window_create_info;

void window_init(engine *, struct window_create_info);
void window_cleanup(engine *);
b8 window_should_close(window *w);
void window_update(window *w);
void window_set_mouse_pos_callback(engine *e, mouse_pos_callback_t *callback);

#endif // WINDOW_H
