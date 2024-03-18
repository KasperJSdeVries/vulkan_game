#include "command_buffer.h"
#include "context.h"
#include "darray.h"
#include "defines.h"
#include "device.h"
#include "pipeline.h"
#include "types.h"
#include "vulkan/vulkan_core.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <cglm/struct.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1280
#define HEIGHT 720

static void copy_buffer(const context *context,
                        VkBuffer src_buffer,
                        VkBuffer dst_buffer,
                        VkDeviceSize size);

typedef struct {
    mat4s model;
    mat4s view;
    mat4s projection;
} UniformBufferObject;

typedef struct {
    u32 vertex_count;
    vec3s *vertices;
    u32 *indices;
    u64 triangles_length;
} Mesh;

static GLFWwindow *create_window() {
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "Failed to initialize GLFW!\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "game", NULL, NULL);

    return window;
}

int main() {
    GLFWwindow *window = create_window();
    context render_context = context_new(window);

    f32 delta_time;
    f32 last_time;
    f32 last_second = glfwGetTime();
    u16 frames = 0;

    while (!glfwWindowShouldClose(window)) {
        f32 current_time = glfwGetTime();
        delta_time = current_time - last_time;
        last_time = current_time;
        frames++;
        if (current_time >= last_second + 1.0f) {
            printf("%d\n", frames);
            frames = 0;
            last_second = current_time;
        }

        glfwPollEvents();

        VkCommandBuffer command_buffer = context_begin_frame(&render_context);

        context_end_frame(&render_context);
    }

    context_end_main_loop(&render_context);

    context_cleanup(&render_context);
}

static void copy_buffer(const context *context,
                        VkBuffer src_buffer,
                        VkBuffer dst_buffer,
                        VkDeviceSize size) {
    VkCommandBuffer command_buffer = begin_single_time_commands(context);

    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size,
    };

    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    end_single_time_commands(context, command_buffer);
}
