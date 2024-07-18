#include "engine.h"
#include "containers/darray.h"
#include "renderer/context.h"
#include "window.h"

#include <string.h>

engine *engine_create(void) {
    engine *e = malloc(sizeof(*e));
    memset(e, 0, sizeof(*e));

    e->user_systems = darray_create(user_system);

    return e;
}

b8 engine_should_keep_running(engine *e) {
    if (e->window) {
        return window_should_close(e->window);
    }

    return true;
}

void engine_update(engine *e) {
    f64 current_time = glfwGetTime();
    f64 delta_time = current_time - e->last_time;
    e->last_time = current_time;
    e->frames++;
    if (current_time >= (f64)(e->last_second + 1)) {
        printf("%d\n", e->frames);
        e->frames = 0;
        e->last_second = current_time;
    }

    for (u32 i = 0; i < darray_length(e->user_systems); i++) {
        e->user_systems[i].update(delta_time, e->user_systems[i].data);
    }

    VkCommandBuffer command_buffer = context_begin_frame(&e->render_context);

    for (u32 i = 0; i < darray_length(e->user_systems); i++) {
        e->user_systems[i].render(e->user_systems[i].data,
                                  e->render_context.current_frame,
                                  command_buffer);
    }

    context_end_frame(&e->render_context);
}
void engine_cleanup(engine *e) {
    for (u32 i = 0; i < darray_length(e->user_systems); i++) {
        e->user_systems[i].cleanup(e, e->user_systems->data);
        free(e->user_systems[i].data);
    }
    darray_destroy(e->user_systems);

    if (e->window) {
        window_cleanup(e);
    }

    free(e);
}
