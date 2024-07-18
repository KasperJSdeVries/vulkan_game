#ifndef TYPES_H
#define TYPES_H

#include "defines.h"
#include "vulkan/vulkan_core.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>

#define VK_CHECK(fun)                                                                              \
    if (fun != VK_SUCCESS) {                                                                       \
        fprintf(stderr, "%s:%d: \"%s\"\n", __FILE_NAME__, __LINE__, #fun);                         \
        exit(EXIT_FAILURE);                                                                        \
    }

#define MAX_FRAMES_IN_FLIGHT 3

struct context;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    u32 format_count;
    VkSurfaceFormatKHR *formats;
    u32 present_mode_count;
    VkPresentModeKHR *present_modes;
} swapchain_support_info;

typedef struct device {
    VkDevice logical_device;
    VkPhysicalDevice physical_device;
    swapchain_support_info swapchain_support;

    i32 graphics_queue_index;
    i32 present_queue_index;
    i32 transfer_queue_index;
    i32 compute_queue_index;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkCommandPool graphics_command_pool;

    VkFormat depth_format;
} device;

typedef struct {
    VkSurfaceFormatKHR image_format;

    u8 max_frames_in_flight;

    VkSwapchainKHR handle;

    u32 image_count;

    VkImage *images;
    VkImageView *image_views;

    VkFramebuffer *framebuffers;

    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;
} swapchain;

typedef struct {
    const struct context *context;

    VkShaderModule vertex_shader_module;
    VkShaderModule fragment_shader_module;
    VkPipelineShaderStageCreateInfo shader_stages[2];

    VkVertexInputBindingDescription *vertex_input_bindings;     // darray
    VkVertexInputAttributeDescription *vertex_input_attributes; // darray

    u64 ubo_size;
    VkCullModeFlags cull_mode;
    VkPrimitiveTopology topology;
    b8 enable_alpha_blending;

    VkPushConstantRange *push_constant_ranges; // darray
} pipeline_builder;

typedef struct {
    VkPipeline handle;

    VkPipelineLayout layout;

    VkDescriptorSetLayout global_descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet global_descriptor_sets[MAX_FRAMES_IN_FLIGHT];

    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_buffer_memory;
    void *uniform_buffer_mapped;
} pipeline;

typedef struct context {
    u32 framebuffer_width;
    u32 framebuffer_height;
    u64 framebuffer_size_generation;
    u64 framebuffer_size_last_generation;

    VkInstance instance;
    VkSurfaceKHR surface;

    b8 framebuffer_resized;

#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif

    device device;

    swapchain swapchain;

    VkCommandBuffer graphics_command_buffers[MAX_FRAMES_IN_FLIGHT];

    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

    u32 image_index;
    u32 current_frame;

    VkRenderPass render_pass;

    i32 (*find_memory_index)(const struct context *context, u32 type_filter, u32 property_flags);
} context;

#endif // TYPES_H
