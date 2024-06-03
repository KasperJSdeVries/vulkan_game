#include "pipeline.h"

#include "context.h"
#include "darray.h"
#include "defines.h"

#include "device.h"
#include "types.h"
#include "vulkan/vulkan_core.h"
#include <stdio.h>
#include <vulkan/vulkan.h>

#include <stdlib.h>

static VkShaderModule create_shader_module(VkDevice device, const u32 *code, u64 code_size);
static u32 *read_file(const char *file_name, u64 *out_size);

/**************************************************************************************************
 * public functions                                                                               *
 **************************************************************************************************/

pipeline_builder pipeline_builder_new(const context *context) {
    pipeline_builder builder = {
        .context = context,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .vertex_input_attributes = darray_create(VkVertexInputAttributeDescription),
        .vertex_input_bindings = darray_create(VkVertexInputBindingDescription),
        .push_constant_ranges = darray_create(VkPushConstantRange),
    };

    return builder;
}

void pipeline_builder_set_shaders(pipeline_builder *builder,
                                  const char *vertex_shader_path,
                                  const char *fragment_shader_path) {
    u64 vertex_shader_size;
    u32 *vertex_shader_code = read_file(vertex_shader_path, &vertex_shader_size);
    if (builder->vertex_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(builder->context->device.logical_device,
                              builder->vertex_shader_module,
                              NULL);
    }
    builder->vertex_shader_module = create_shader_module(builder->context->device.logical_device,
                                                         vertex_shader_code,
                                                         vertex_shader_size);
    free(vertex_shader_code);

    VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = builder->vertex_shader_module,
        .pName = "main",
    };

    builder->shader_stages[0] = vertex_shader_stage_info;

    u64 fragment_shader_size;
    u32 *fragment_shader_code = read_file(fragment_shader_path, &fragment_shader_size);
    if (builder->fragment_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(builder->context->device.logical_device,
                              builder->fragment_shader_module,
                              NULL);
    }
    builder->fragment_shader_module = create_shader_module(builder->context->device.logical_device,
                                                           fragment_shader_code,
                                                           fragment_shader_size);
    free(fragment_shader_code);

    VkPipelineShaderStageCreateInfo fragment_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = builder->fragment_shader_module,
        .pName = "main",
    };

    builder->shader_stages[1] = fragment_shader_stage_info;
}

void pipeline_builder_add_input_binding(pipeline_builder *builder,
                                        u32 binding,
                                        u64 stride,
                                        VkVertexInputRate input_rate) {
    VkVertexInputBindingDescription description = {
        .binding = binding,
        .stride = stride,
        .inputRate = input_rate,
    };

    darray_push(builder->vertex_input_bindings, description);
}

void pipeline_builder_add_input_attribute(pipeline_builder *builder,
                                          u32 binding,
                                          u32 location,
                                          VkFormat format,
                                          u32 offset) {
    VkVertexInputAttributeDescription description = {
        .binding = binding,
        .location = location,
        .format = format,
        .offset = offset,
    };

    darray_push(builder->vertex_input_attributes, description);
}

void pipeline_builder_set_ubo_size(pipeline_builder *builder, u64 ubo_size) {
    builder->ubo_size = ubo_size;
}

void pipeline_builder_set_topology(pipeline_builder *builder, VkPrimitiveTopology topology) {
    builder->topology = topology;
}

void pipeline_builder_add_push_constant(pipeline_builder *builder,
                                        VkShaderStageFlagBits shader_stage,
                                        u32 size) {
    VkPushConstantRange range = {
        .stageFlags = shader_stage,
        .offset = 0,
        .size = size,
    };

    darray_push(builder->push_constant_ranges, range);
}

pipeline pipeline_builder_build(pipeline_builder *builder, VkRenderPass render_pass) {
    pipeline pipeline = {0};

    if (builder->ubo_size) {
        VkDescriptorSetLayoutBinding layout_bindings[] = {
            {
                .binding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImmutableSamplers = NULL,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
        };

        VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = sizeof(VkDescriptorSetLayoutBinding) / sizeof(layout_bindings),
            .pBindings = layout_bindings,
        };

        VK_CHECK(vkCreateDescriptorSetLayout(builder->context->device.logical_device,
                                             &layout_info,
                                             NULL,
                                             &pipeline.global_descriptor_set_layout));
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = darray_length(builder->vertex_input_bindings),
        .pVertexBindingDescriptions = builder->vertex_input_bindings,
        .vertexAttributeDescriptionCount = darray_length(builder->vertex_input_attributes),
        .pVertexAttributeDescriptions = builder->vertex_input_attributes,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = builder->topology,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    uint32_t dynamic_state_count = sizeof(dynamic_states) / sizeof(VkDynamicState);

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_state_count,
        .pDynamicStates = dynamic_states,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisample_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = darray_length(builder->push_constant_ranges),
        .pPushConstantRanges = builder->push_constant_ranges,
    };

    if (builder->ubo_size) {
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &pipeline.global_descriptor_set_layout;
    }

    VK_CHECK(vkCreatePipelineLayout(builder->context->device.logical_device,
                                    &pipeline_layout_create_info,
                                    NULL,
                                    &pipeline.layout));

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = builder->shader_stages,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pDynamicState = &dynamic_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pColorBlendState = &color_blend_state,
        .pDepthStencilState = &depth_stencil_state,
        .layout = pipeline.layout,
        .renderPass = render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VK_CHECK(vkCreateGraphicsPipelines(builder->context->device.logical_device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &create_info,
                                       NULL,
                                       &pipeline.handle));

    if (builder->ubo_size != 0) {
        context_create_buffer(builder->context,
                              builder->ubo_size,
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &pipeline.uniform_buffer,
                              &pipeline.uniform_buffer_memory);
        vkMapMemory(builder->context->device.logical_device,
                    pipeline.uniform_buffer_memory,
                    0,
                    builder->ubo_size,
                    0,
                    &pipeline.uniform_buffer_mapped);

        VkDescriptorPoolSize pool_sizes[] = {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT,
            },
        };

        VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize),
            .pPoolSizes = pool_sizes,
            .maxSets = MAX_FRAMES_IN_FLIGHT,
        };

        VK_CHECK(vkCreateDescriptorPool(builder->context->device.logical_device,
                                        &pool_info,
                                        NULL,
                                        &pipeline.descriptor_pool));

        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            layouts[i] = pipeline.global_descriptor_set_layout;
        }

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pipeline.descriptor_pool,
            .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
            .pSetLayouts = layouts,
        };

        VK_CHECK(vkAllocateDescriptorSets(builder->context->device.logical_device,
                                          &alloc_info,
                                          pipeline.global_descriptor_sets));

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo buffer_info = {
                .buffer = pipeline.uniform_buffer,
                .offset = 0,
                .range = builder->ubo_size,
            };

            VkWriteDescriptorSet descriptor_writes[] = {
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = pipeline.global_descriptor_sets[i],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .pBufferInfo = &buffer_info,
                },
            };

            vkUpdateDescriptorSets(builder->context->device.logical_device,
                                   sizeof(descriptor_writes) / sizeof(VkWriteDescriptorSet),
                                   descriptor_writes,
                                   0,
                                   NULL);
        }
    }

    vkDestroyShaderModule(builder->context->device.logical_device,
                          builder->vertex_shader_module,
                          NULL);
    vkDestroyShaderModule(builder->context->device.logical_device,
                          builder->fragment_shader_module,
                          NULL);

    darray_destroy(builder->vertex_input_attributes);
    darray_destroy(builder->vertex_input_bindings);

    return pipeline;
}

void pipeline_bind(const pipeline *pipeline, VkCommandBuffer command_buffer, u32 frame_index) {
    if (pipeline->uniform_buffer != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(command_buffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->layout,
                                0,
                                1,
                                &pipeline->global_descriptor_sets[frame_index],
                                0,
                                NULL);
    } else {
    }
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
}

void pipeline_destroy(pipeline *pipeline, device *device) {
    if (pipeline->uniform_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device->logical_device, pipeline->uniform_buffer, NULL);
        vkFreeMemory(device->logical_device, pipeline->uniform_buffer_memory, NULL);

        vkDestroyDescriptorPool(device->logical_device, pipeline->descriptor_pool, NULL);
        vkDestroyDescriptorSetLayout(device->logical_device,
                                     pipeline->global_descriptor_set_layout,
                                     NULL);
    }

    vkDestroyPipeline(device->logical_device, pipeline->handle, NULL);
    vkDestroyPipelineLayout(device->logical_device, pipeline->layout, NULL);
}

/**************************************************************************************************
 * private functions                                                                              *
 **************************************************************************************************/

static VkShaderModule create_shader_module(VkDevice device, const u32 *code, u64 code_size) {
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code_size,
        .pCode = code,
    };

    VkShaderModule shader_module;

    VK_CHECK(vkCreateShaderModule(device, &create_info, NULL, &shader_module));

    return shader_module;
}

static u32 *read_file(const char *file_name, u64 *out_size) {
    FILE *fp = fopen(file_name, "rb");

    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0, SEEK_END);
    *out_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    u32 *buffer = malloc(*out_size);
    if (fread(buffer, sizeof(char), *out_size, fp) != *out_size) {
        fprintf(stderr, "Failed to read whole file: %s\n", file_name);
    }

    fclose(fp);
    return buffer;
}
