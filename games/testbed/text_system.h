#ifndef TEXT_SYSTEM_H
#define TEXT_SYSTEM_H

#include <engine.h>
#include <renderer/types.h>

typedef struct text_system {
    pipeline pipeline;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
} text_system;

b8 text_system_init(engine *e, void **data_ptr);
void text_system_cleanup(engine *e, void *renderer);
void text_system_render(void *renderer, u32 current_frame, VkCommandBuffer command_buffer);

#endif // TEXT_SYSTEM_H
