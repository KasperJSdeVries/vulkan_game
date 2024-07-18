#include "text_system.h"

#include <engine.h>
#include <renderer/context.h>
#include <renderer/pipeline.h>

#include <string.h>

static void text_renderer_setup_buffers(text_system *renderer, context *render_context) {
    VkDeviceSize vertex_buffer_size = sizeof(vec2s) * 2 * 3;

    VkBuffer vertex_staging_buffer;
    VkDeviceMemory vertex_staging_buffer_memory;

    context_create_buffer(render_context,
                          vertex_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &vertex_staging_buffer,
                          &vertex_staging_buffer_memory);

    context_create_buffer(render_context,
                          vertex_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &renderer->vertex_buffer,
                          &renderer->vertex_buffer_memory);

    void *vertex_staging_buffer_memory_mapped;
    vkMapMemory(render_context->device.logical_device,
                vertex_staging_buffer_memory,
                0,
                vertex_buffer_size,
                0,
                &vertex_staging_buffer_memory_mapped);

    vec2s buf[] = {
        {{-0.5, 0}},
        {{0, 0}},
        {{0, -0.5}},
        {{0.5, 0}},
        {{0.5, 0}},
        {{1, 1}},
    };

    memcpy((void *)((u64)vertex_staging_buffer_memory_mapped), buf, sizeof(buf));

    context_copy_buffer(render_context,
                        vertex_staging_buffer,
                        renderer->vertex_buffer,
                        vertex_buffer_size);

    vkDestroyBuffer(render_context->device.logical_device, vertex_staging_buffer, NULL);
    vkFreeMemory(render_context->device.logical_device, vertex_staging_buffer_memory, NULL);
}

b8 text_system_init(engine *e, void **data_ptr) {
    *data_ptr = malloc(sizeof(text_system));
    if (*data_ptr == NULL) {
        return false;
    }

    text_system *ts = *data_ptr;

    pipeline_builder builder = pipeline_builder_new(&e->render_context);
    pipeline_builder_set_shaders(&builder, "shaders/text.vert.spv", "shaders/text.frag.spv");
    pipeline_builder_add_input_binding(&builder, 0, sizeof(vec2s) * 2, VK_VERTEX_INPUT_RATE_VERTEX);
    pipeline_builder_add_input_attribute(&builder, 0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
    pipeline_builder_add_input_attribute(&builder, 0, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(vec2s));
    pipeline_builder_set_cull_mode(&builder, VK_CULL_MODE_NONE);
    pipeline_builder_set_alpha_blending(&builder, true);

    ts->pipeline = pipeline_builder_build(&builder, e->render_context.render_pass);

    text_renderer_setup_buffers(ts, &e->render_context);

    return true;
}

void text_system_cleanup(engine *e, void *ptr) {
    text_system *ts = ptr;
    vkDestroyBuffer(e->render_context.device.logical_device, ts->vertex_buffer, NULL);
    vkFreeMemory(e->render_context.device.logical_device, ts->vertex_buffer_memory, NULL);

    pipeline_destroy(&ts->pipeline, &e->render_context.device);
}

void text_system_render(void *ptr, u32 current_frame, VkCommandBuffer command_buffer) {
    text_system *ts = ptr;
    pipeline_bind(&ts->pipeline, command_buffer, current_frame);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &ts->vertex_buffer, offsets);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);
}
