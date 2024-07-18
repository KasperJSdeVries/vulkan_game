#ifndef ENGINE_H
#define ENGINE_H

#include "defines.h"
#include "renderer/types.h"

struct engine;

typedef void(mouse_pos_callback_t)(f64 x, f64 y);

typedef struct window {
    GLFWwindow *handle;
    mouse_pos_callback_t *mouse_pos_callback;
} window;

typedef void(system_cleanup_t)(struct engine *, void *);
typedef b8(system_update_t)(f64 delta_time, void *);
typedef void(system_render_t)(void *, u32 current_frame, VkCommandBuffer command_buffer);

typedef struct user_system {
    system_cleanup_t *cleanup;
    system_update_t *update;
    system_render_t *render;
    void *data;
    b8 active;
} user_system;

typedef struct engine {
    window *window;
    context render_context;
    user_system *user_systems; // darray

    f32 last_time;
    u64 last_second;
    u16 frames;
} engine;

engine *engine_create(void);
b8 engine_should_keep_running(engine *);
void engine_update(engine *);
void engine_cleanup(engine *);

#endif // ENGINE_H
