#ifndef CONTEXT_H
#define CONTEXT_H

#include "types.h"

context context_new(GLFWwindow *window);

void context_on_resized(context *context, u32 width, u32 height);

void context_begin_main_loop(context *context);
VkCommandBuffer context_begin_frame(context *context);
void context_end_frame(context *context);
void context_end_main_loop(context *context);

void context_cleanup(context *context);

void context_create_buffer(const context *context,
                           VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties,
                           VkBuffer *buffer,
                           VkDeviceMemory *buffer_memory);

#endif // CONTEXT_H
