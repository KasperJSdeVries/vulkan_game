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

typedef struct {
    Mesh mesh;
    i32 resolution;
    vec3s local_up;
    vec3s axis_a;
    vec3s axis_b;
} TerrainFace;

static TerrainFace create_terrain_face(int resolution, vec3s local_up) {
    TerrainFace terrain_face;
    terrain_face.resolution = resolution;
    terrain_face.local_up = local_up;
    terrain_face.axis_a = (vec3s){local_up.y, local_up.z, local_up.x};
    terrain_face.axis_b = glms_vec3_cross(local_up, terrain_face.axis_a);

    return terrain_face;
}

static void terrain_face_construct_mesh(TerrainFace *terrain_face) {
    if (terrain_face->mesh.vertices) {
        free(terrain_face->mesh.vertices);
    }
    if (terrain_face->mesh.indices) {
        free(terrain_face->mesh.indices);
    }

    terrain_face->mesh.vertex_count = terrain_face->resolution * terrain_face->resolution;

    terrain_face->mesh.vertices = malloc(terrain_face->mesh.vertex_count * sizeof(vec3s));

    terrain_face->mesh.triangles_length =
        (terrain_face->resolution - 1) * (terrain_face->resolution - 1) * 6;

    terrain_face->mesh.indices = malloc(terrain_face->mesh.triangles_length * sizeof(u32));

    u32 triangle_index = 0;
    for (int y = 0; y < terrain_face->resolution; y++) {
        for (int x = 0; x < terrain_face->resolution; x++) {
            u32 i = x + y * terrain_face->resolution;
            vec2s percent = glms_vec2_divs((vec2s){x, y}, (terrain_face->resolution - 1));
            // local_up
            // + (percent.x - 0.5f) * 2.0f * axis_a
            // + (percent.y - 0.5f) * 2.0f * axis_b
            vec3s point_on_unit_cube = glms_vec3_add(
                terrain_face->local_up,
                glms_vec3_add(glms_vec3_scale(terrain_face->axis_a, (percent.x - 0.5f) * 2.0f),
                              glms_vec3_scale(terrain_face->axis_b, (percent.y - 0.5f) * 2.0f)));
            vec3s point_on_unit_sphere = glms_vec3_normalize(point_on_unit_cube);
            terrain_face->mesh.vertices[i] = point_on_unit_sphere;

            if (x != terrain_face->resolution - 1 && y != terrain_face->resolution - 1) {
                terrain_face->mesh.indices[triangle_index] = i;
                terrain_face->mesh.indices[triangle_index + 1] = i + terrain_face->resolution + 1;
                terrain_face->mesh.indices[triangle_index + 2] = i + terrain_face->resolution;

                terrain_face->mesh.indices[triangle_index + 3] = i;
                terrain_face->mesh.indices[triangle_index + 4] = i + 1;
                terrain_face->mesh.indices[triangle_index + 5] = i + terrain_face->resolution + 1;
                triangle_index += 6;
            }
        }
    }
}

#define FACES_PER_PLANET 6

typedef struct {
    TerrainFace terrain_faces[FACES_PER_PLANET];
} Planet;

static Planet create_planet() {
    Planet planet = {};
    int resolution = 4;
    vec3s directions[] = {
        {0, 0, 1},
        {0, 0, -1},
        {0, 1, 0},
        {0, -1, 0},
        {1, 0, 0},
        {-1, 0, 0},
    };

    for (int i = 0; i < FACES_PER_PLANET; i++) {
        planet.terrain_faces[i] = create_terrain_face(resolution, directions[i]);
    }

    return planet;
};

static void planet_generate_meshes(Planet *planet) {
    for (int i = 0; i < FACES_PER_PLANET; i++) {
        terrain_face_construct_mesh(&planet->terrain_faces[i]);
    }
};

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
    pipeline_builder_add_input_binding(&planet_pipeline_builder,
                                       0,
                                       sizeof(vec3s),
                                       VK_VERTEX_INPUT_RATE_VERTEX);
    pipeline_builder_add_input_attribute(&planet_pipeline_builder,
                                         0,
                                         0,
                                         VK_FORMAT_R32G32B32_SFLOAT,
                                         0);
    pipeline_builder_set_shaders(&planet_pipeline_builder,
                                 "shaders/simple.vert.spv",
                                 "shaders/simple.frag.spv");
    pipeline planet_pipeline =
        pipeline_builder_build(&planet_pipeline_builder, render_context.render_pass);

    Planet planet = create_planet();
    planet_generate_meshes(&planet);

    VkDeviceSize vertex_buffer_size =
        sizeof(vec3s) * (planet.terrain_faces[0].mesh.vertex_count + 1);

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

    VkBuffer vertex_buffers[FACES_PER_PLANET];
    VkDeviceMemory vertex_buffer_memories[FACES_PER_PLANET];
    for (u32 i = 0; i < FACES_PER_PLANET; i++) {
        context_create_buffer(&render_context,
                              vertex_buffer_size,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              &vertex_buffers[i],
                              &vertex_buffer_memories[i]);

        for (u32 j = 0; j < planet.terrain_faces[i].mesh.vertex_count; j++) {
            vec3s vertex = planet.terrain_faces[i].mesh.vertices[j];
        }

        memcpy(vertex_staging_buffer_memory_mapped,
               planet.terrain_faces[i].mesh.vertices,
               vertex_buffer_size);

        copy_buffer(&render_context, vertex_staging_buffer, vertex_buffers[i], vertex_buffer_size);
    }

    VkDeviceSize index_buffer_size = sizeof(u32) * planet.terrain_faces[0].mesh.triangles_length;

    VkBuffer index_staging_buffer;
    VkDeviceMemory index_staging_buffer_memory;

    VkBuffer index_buffers;
    VkDeviceMemory index_buffer_memories;

    context_create_buffer(&render_context,
                          index_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &index_staging_buffer,
                          &index_staging_buffer_memory);

    void *index_staging_buffer_memory_mapped;
    vkMapMemory(render_context.device.logical_device,
                index_staging_buffer_memory,
                0,
                index_buffer_size,
                0,
                &index_staging_buffer_memory_mapped);

    context_create_buffer(&render_context,
                          index_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &index_buffers,
                          &index_buffer_memories);

    memcpy(index_staging_buffer_memory_mapped,
           planet.terrain_faces[0].mesh.indices,
           index_buffer_size);

    copy_buffer(&render_context, index_staging_buffer, index_buffers, index_buffer_size);

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

        vkCmdBindIndexBuffer(command_buffer, index_buffers, 0, VK_INDEX_TYPE_UINT32);

        for (u32 i = 0; i < FACES_PER_PLANET; i++) {
            VkBuffer buffers[] = {vertex_buffers[i]};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, buffers, offsets);

            vkCmdDrawIndexed(command_buffer,
                             planet.terrain_faces[i].mesh.triangles_length,
                             1,
                             0,
                             0,
                             0);
        }

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

    for (u32 i = 0; i < FACES_PER_PLANET; i++) {
        vkDestroyBuffer(render_context.device.logical_device, vertex_buffers[i], NULL);
        vkFreeMemory(render_context.device.logical_device, vertex_buffer_memories[i], NULL);
    }

    vkDestroyBuffer(render_context.device.logical_device, index_staging_buffer, NULL);
    vkFreeMemory(render_context.device.logical_device, index_staging_buffer_memory, NULL);

    vkDestroyBuffer(render_context.device.logical_device, index_buffers, NULL);
    vkFreeMemory(render_context.device.logical_device, index_buffer_memories, NULL);

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
