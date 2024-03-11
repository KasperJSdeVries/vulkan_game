#ifndef APPLICATION_H
#define APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <cglm/util.h>

#include "defines.h"

#define MAX_FRAMES_IN_FLIGHT 2

#define VK_CHECK(fun)                                                                              \
    if (fun != VK_SUCCESS) {                                                                       \
        fprintf(stderr, "%s:%d: \"%s\"\n", __FILE_NAME__, __LINE__, #fun);                         \
        exit(EXIT_FAILURE);                                                                        \
    }

typedef struct {
    mat4s model;
    mat4s view;
    mat4s projection;
} UniformBufferObject;

typedef struct {
    GLFWwindow *window;
    VkSurfaceKHR surface;

    double time;
    double delta_time;

    VkInstance instance;

    VkDebugUtilsMessengerEXT debug_messenger;

    VkPhysicalDevice physical_device;
    VkDevice device;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;
    uint32_t swapchain_image_count;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    VkFramebuffer *swapchain_framebuffers;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];

    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

    uint32_t current_frame;
    bool framebuffer_resized;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;

    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;

    VkBuffer uniform_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory uniform_buffers_memory[MAX_FRAMES_IN_FLIGHT];
    void *uniform_buffers_mapped[MAX_FRAMES_IN_FLIGHT];

    VkImage texture_image;
    VkDeviceMemory texture_image_memory;
    VkImageView texture_image_view;
    VkSampler texture_sampler;

    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;
} Application;

typedef struct {
    vec3s position;
    vec3s color;
    vec2s texture_coordinate;
} Vertex;

void init_window(Application *app);
void init_vulkan(Application *app);
void main_loop(Application *app);
void cleanup(Application *app);

#endif // APPLICATION_H
