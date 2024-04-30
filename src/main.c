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
    f32 altitude;
} HeightmapVertex;

typedef struct {
    f32 vertices_per_run;
    f32 vertices_per_run_not_degenerate;
} Params;

#define HEIGHTMAP_SIZE 32
#define HEIGHTMAP_SIZE_SQUARED (HEIGHTMAP_SIZE * HEIGHTMAP_SIZE)

#define VERTICES_PER_RUN (HEIGHTMAP_SIZE * 2 + 4)
#define VERTICES_PER_CHUNK (VERTICES_PER_RUN * HEIGHTMAP_SIZE)
#define VERTICES_PER_RUN_NOT_DEGENERATE (VERTICES_PER_RUN - 3)

f32 get_height(i32 x, i32 z) { return sinf(x * 0.5f) + cosf(z * 0.25f) * 2.0f; }

void generate_buffer(HeightmapVertex **buffer, u64 *buffer_size) {
    *buffer_size = VERTICES_PER_CHUNK * sizeof(HeightmapVertex);
    *buffer = calloc(VERTICES_PER_CHUNK, sizeof(HeightmapVertex));

    f32 extra_height = 0.0f;
    u64 index = 0;
    for (i32 z = 0; z < HEIGHTMAP_SIZE; z++) {
        i32 x = 0;

        f32 altitude0 = get_height(x, z);
        f32 altitude1 = get_height(x, z + 1);
        f32 altitude2 = get_height(x + 1, z);

        // is degenerate
        (*buffer)[index++].altitude = altitude0;

        // first triangle
        (*buffer)[index++].altitude = altitude0;
        (*buffer)[index++].altitude = altitude1;
        (*buffer)[index++].altitude = altitude2;

        x += 1;
        f32 altitude = get_height(x, z + 1);
        (*buffer)[index++].altitude = altitude;

        x += 1;
        for (; x <= HEIGHTMAP_SIZE; x++) {
            altitude = get_height(x, z);
            (*buffer)[index++].altitude = altitude;

            altitude = get_height(x, z + 1);
            (*buffer)[index++].altitude = altitude;
        }

        // degenerate
        altitude = get_height(x - 1, z + 1);
        (*buffer)[index++].altitude = altitude;
    }
}

f32 pitch = 0, yaw = -90.0f;
f32 last_x = 400, last_y = 300;
static void mouse_callback(GLFWwindow *window, double x_position, double y_position) {
    f32 x_offset = x_position - last_x;
    f32 y_offset = last_y - y_position;
    last_x = x_position;
    last_y = y_position;

    const f32 sensitivity = 0.01f;
    x_offset *= sensitivity;
    y_offset *= sensitivity;

    yaw += x_offset;
    pitch += y_offset;

    pitch = CLAMP(pitch, -89.0f, 89.0f);
}

static GLFWwindow *create_window() {
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "Failed to initialize GLFW!\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "game", NULL, NULL);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    return window;
}

typedef struct {
    vec3s position;
    vec3s front;
    vec3s up;
} Camera;

static void process_input(GLFWwindow *window, Camera *camera, f32 delta_time);

int main() {
    GLFWwindow *window = create_window();
    context render_context = context_new(window);

    pipeline_builder planet_pipeline_builder = pipeline_builder_new(&render_context);
    pipeline_builder_set_ubo_size(&planet_pipeline_builder, sizeof(UniformBufferObject));
    pipeline_builder_set_topology(&planet_pipeline_builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    pipeline_builder_add_input_binding(&planet_pipeline_builder, 0, sizeof(HeightmapVertex));
    pipeline_builder_add_input_attribute(&planet_pipeline_builder,
                                         0,
                                         0,
                                         VK_FORMAT_R32_SFLOAT,
                                         offsetof(HeightmapVertex, altitude));
    pipeline_builder_add_push_constant(&planet_pipeline_builder,
                                       VK_SHADER_STAGE_VERTEX_BIT,
                                       sizeof(Params));
    pipeline_builder_set_shaders(&planet_pipeline_builder,
                                 "shaders/terrain_vectors.vert.spv",
                                 "shaders/terrain_vectors.frag.spv");
    pipeline planet_pipeline =
        pipeline_builder_build(&planet_pipeline_builder, render_context.render_pass);

    u64 vertex_buffer_size;
    HeightmapVertex *vertex_buffer_data;
    generate_buffer(&vertex_buffer_data, &vertex_buffer_size);

    VkBuffer vertex_staging_buffer;
    VkDeviceMemory vertex_staging_buffer_memory;

    context_create_buffer(&render_context,
                          vertex_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &vertex_staging_buffer,
                          &vertex_staging_buffer_memory);
    void *vertex_staging_buffer_memory_mapped;
    vkMapMemory(render_context.device.logical_device,
                vertex_staging_buffer_memory,
                0,
                vertex_buffer_size,
                0,
                &vertex_staging_buffer_memory_mapped);

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    context_create_buffer(&render_context,
                          vertex_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &vertex_buffer,
                          &vertex_buffer_memory);

    memcpy(vertex_staging_buffer_memory_mapped, vertex_buffer_data, vertex_buffer_size);

    copy_buffer(&render_context, vertex_staging_buffer, vertex_buffer, vertex_buffer_size);

    context_begin_main_loop(&render_context);

    Camera camera = {
        {0.0, 0.0, 5.0},
        {0.0, 0.0, -1.0},
        {0.0, 1.0, 0.0},
    };

    f32 delta_time;
    f32 last_time;
    f32 last_second = glfwGetTime();
    u16 frames = 0;

    Params params = {
        .vertices_per_run = VERTICES_PER_RUN,
        .vertices_per_run_not_degenerate = VERTICES_PER_RUN_NOT_DEGENERATE,
    };

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

        process_input(window, &camera, delta_time);

        VkCommandBuffer command_buffer = context_begin_frame(&render_context);

        pipeline_bind(&planet_pipeline, command_buffer, render_context.current_frame);

        VkBuffer buffers[] = {vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, buffers, offsets);

        vkCmdPushConstants(command_buffer,
                           planet_pipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(Params),
                           &params);

        vkCmdDraw(command_buffer, VERTICES_PER_CHUNK, 1, 0, 0);

        context_end_frame(&render_context);

        vec3s direction = {
            cos(glm_rad(yaw)) * cos(glm_rad(pitch)),
            sin(glm_rad(pitch)),
            sin(glm_rad(yaw)) * cos(glm_rad(pitch)),
        };
        camera.front = glms_vec3_normalize(direction);

        UniformBufferObject ubo = {
            .model = glms_mat4_identity(),
            // https://learnopengl.com/Getting-started/Camera
            .view = glms_lookat(camera.position,
                                glms_vec3_add(camera.position, camera.front),
                                camera.up),
            .projection = glms_perspective(glm_rad(45.0f),
                                           (float)render_context.framebuffer_width /
                                               (float)render_context.framebuffer_height,
                                           0.1f,
                                           1000.0f),
        };

        ubo.projection.m11 *= -1;

        memcpy(planet_pipeline.uniform_buffer_mapped, &ubo, sizeof(ubo));
    }

    context_end_main_loop(&render_context);

    vkDestroyBuffer(render_context.device.logical_device, vertex_staging_buffer, NULL);
    vkFreeMemory(render_context.device.logical_device, vertex_staging_buffer_memory, NULL);

    vkDestroyBuffer(render_context.device.logical_device, vertex_buffer, NULL);
    vkFreeMemory(render_context.device.logical_device, vertex_buffer_memory, NULL);

    pipeline_destroy(&planet_pipeline, &render_context.device);
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

static void process_input(GLFWwindow *window, Camera *camera, f32 delta_time) {
    const float camera_speed = delta_time * 2.5f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera->position =
            glms_vec3_add(camera->position, glms_vec3_scale(camera->front, camera_speed));
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera->position =
            glms_vec3_sub(camera->position, glms_vec3_scale(camera->front, camera_speed));
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera->position = glms_vec3_sub(
            camera->position,
            glms_vec3_scale(glms_vec3_normalize(glms_vec3_cross(camera->front, camera->up)),
                            camera_speed));
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera->position = glms_vec3_add(
            camera->position,
            glms_vec3_scale(glms_vec3_normalize(glms_vec3_cross(camera->front, camera->up)),
                            camera_speed));
    }
}
