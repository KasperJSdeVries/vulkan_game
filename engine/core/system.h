#ifndef SYSTEM_H
#define SYSTEM_H

#include "engine.h"

typedef u32 system_handle;

typedef struct system_create_info {
    b8 (*init)(engine *, void **);
    system_cleanup_t *cleanup;
    system_update_t *update;
    system_render_t *render;
} system_create_info;

system_handle engine_add_system(engine *e, system_create_info create_info);

void system_activate(engine *, system_handle);
void system_deactivate(engine *, system_handle);

#endif // SYSTEM_H
