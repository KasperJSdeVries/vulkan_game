#ifndef PIPELINE_H
#define PIPELINE_H

#include "defines.h"
#include "types.h"

#include <vulkan/vulkan.h>

pipeline_builder pipeline_builder_new(const context *context);

void pipeline_builder_set_shaders(pipeline_builder *builder,
                                  const char *vertex_shader_path,
                                  const char *fragment_shader_path);
void pipeline_builder_add_input_binding(pipeline_builder *builder, u32 binding, u64 stride);
void pipeline_builder_add_input_attribute(pipeline_builder *builder,
                                          u32 binding,
                                          u32 location,
                                          VkFormat format,
                                          u32 offset);
void pipeline_builder_set_ubo_size(pipeline_builder *builder, u64 ubo_size);
void pipeline_builder_set_topology(pipeline_builder *builder, VkPrimitiveTopology topology);
void pipeline_builder_add_push_constant(pipeline_builder *builder,
                                        VkShaderStageFlagBits shader_stage,
                                        u32 size);

pipeline pipeline_builder_build(pipeline_builder *builder, VkRenderPass render_pass);

void pipeline_bind(const pipeline *pipeline, VkCommandBuffer command_buffer, u32 frame_index);

void pipeline_destroy(pipeline *pipeline, device *device);

#endif // PIPELINE_H
