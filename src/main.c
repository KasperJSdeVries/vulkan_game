#include "cglm/struct/io.h"
#include "command_buffer.h"
#include "context.h"
#include "darray.h"
#include "defines.h"
#include "device.h"
#include "pipeline.h"
#include "types.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <cglm/struct.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1280
#define HEIGHT 720

typedef struct {
    vec3s position;
    vec3s front;
    vec3s up;
} Camera;

typedef struct {
    mat4s view;
    mat4s projection;
} UniformBufferObject;

typedef struct {
    mat4s model;
    vec3s colour;
} InstanceData;

f32 pitch = 0, yaw = -90.0f;
f32 last_x = 400, last_y = 300;

static GLFWwindow *create_window();
static void copy_buffer(const context *context,
                        VkBuffer src_buffer,
                        VkBuffer dst_buffer,
                        VkDeviceSize size);
static void process_input(GLFWwindow *window, Camera *camera, f32 delta_time);
static void mouse_callback(GLFWwindow *window, double x_position, double y_position);
static vec3s *generate_sphere();

#define GRAVITY 0.0f
#define PARTICLE_SIZE 0.1f
#define BOUNDS_SIZE ((vec3s){16, 8, 12})
#define COLLISION_DAMPING 0.95f
#define PARTICLE_COUNT 1000
#define PARTICLE_SPACING 0.1f
#define PARTICLE_MASS 1.0f
#define SMOOTHING_RADIUS 1.2f
#define TARGET_DENSITY 2.0f
#define PRESSURE_MULTIPLIER 0.5f
#define DRAG 0.00f

typedef struct {
    vec3s position;
    vec3s velocity;
    vec3s colour;
} Particle;

void particle_resolve_collisions(Particle *p) {
    vec3s half_bounds_size = glms_vec3_sub(glms_vec3_divs(BOUNDS_SIZE, 2.0f),
                                           glms_vec3_scale(glms_vec3_one(), PARTICLE_SIZE));

    if (fabsf(p->position.x) > half_bounds_size.x) {
        p->position.x = half_bounds_size.x * glm_signf(p->position.x);
        p->velocity.x *= -1 * COLLISION_DAMPING;
    }
    if (fabsf(p->position.y) > half_bounds_size.y) {
        p->position.y = half_bounds_size.y * glm_signf(p->position.y);
        p->velocity.y *= -1 * COLLISION_DAMPING;
    }
    if (fabsf(p->position.z) > half_bounds_size.z) {
        p->position.z = half_bounds_size.z * glm_signf(p->position.z);
        p->velocity.z *= -1 * COLLISION_DAMPING;
    }
}

f32 smoothing_kernel(f32 radius, f32 distance) {
    if (distance >= radius)
        return 0;

    f32 volume = PI * powf(radius, 4) / 6;
    return (radius - distance) * (radius - distance) / volume;
}

f32 smoothing_kernel_derivative(f32 radius, f32 distance) {
    if (distance >= radius)
        return 0;

    f32 scale = 12 / (PI * powf(radius, 4));
    return (distance - radius) * scale;
}

f32 calculate_density(Particle *particles, vec3s sample_point) {
    f32 density = 0.0;

    u64 particle_count = darray_length(particles);
    for (i32 i = 0; i < particle_count; i++) {
        f32 distance = glms_vec3_distance(particles[i].position, sample_point);
        f32 influence = smoothing_kernel(SMOOTHING_RADIUS, distance);
        density += PARTICLE_MASS * influence;
    }

    return density;
}

f32 convert_density_to_pressure(f32 density) {
    f32 density_error = density - TARGET_DENSITY;
    f32 pressure = density_error * PRESSURE_MULTIPLIER;
    return pressure;
}

f32 calculate_shared_pressure(f32 density_a, f32 density_b) {
    f32 pressure_a = convert_density_to_pressure(density_a);
    f32 pressure_b = convert_density_to_pressure(density_b);
    return (pressure_a + pressure_b) / 2;
}

vec3s calculate_pressure_force(Particle *particles, f32 *densities, u32 particle_index) {
    vec3s pressure_force = glms_vec3_zero();

    u64 particle_count = darray_length(particles);
    for (i32 i = 0; i < particle_count; i++) {
        f32 distance =
            glms_vec3_distance(particles[i].position, particles[particle_index].position);
        vec3s direction = glms_vec3_normalize(
            glms_vec3_sub(particles[i].position, particles[particle_index].position));
        f32 slope = smoothing_kernel_derivative(SMOOTHING_RADIUS, distance);
        f32 density = densities[i];
        f32 shared_pressure = calculate_shared_pressure(density, densities[particle_index]);
        pressure_force = glms_vec3_add(
            pressure_force,
            glms_vec3_scale(direction, shared_pressure * slope * PARTICLE_MASS / density));
    }

    return pressure_force;
}

void particles_update(Particle *particles, f32 delta_time) {
    u64 particle_count = darray_length(particles);
    f32 densities[particle_count];

    for (i32 i = 0; i < particle_count; i++) {
        vec3s gravity = glms_vec3_scale((vec3s){0, -1, 0}, GRAVITY * delta_time);
        particles[i].velocity = glms_vec3_add(particles[i].velocity, gravity);
        densities[i] = calculate_density(particles, particles[i].position);
        f32 diff = TARGET_DENSITY - densities[i];
        particles[i].colour = (vec3s){
            1.0f - fabsf(fmaxf(0, diff)) * 1 / TARGET_DENSITY,
            1.0f - fabsf(fminf(0, diff) + fmaxf(0, diff)) * 1 / TARGET_DENSITY,
            1.0f - fabsf(fminf(0, diff)) * 1 / TARGET_DENSITY,
        };
    }

    for (i32 i = 0; i < particle_count; i++) {
        vec3s pressure_force = calculate_pressure_force(particles, densities, i);
        vec3s pressure_acceleration = glms_vec3_divs(pressure_force, densities[i]);
        particles[i].velocity = glms_vec3_add(particles[i].velocity,
                                              glms_vec3_scale(pressure_acceleration, delta_time));
    }

    for (i32 i = 0; i < particle_count; i++) {
        vec3s gravity = glms_vec3_scale((vec3s){0, -1, 0}, GRAVITY * delta_time);
        particles[i].velocity = glms_vec3_add(particles[i].velocity, gravity);
        particles[i].velocity = glms_vec3_scale(particles[i].velocity, 1 - DRAG * delta_time);
        particles[i].position = glms_vec3_add(particles[i].position,
                                              glms_vec3_scale(particles[i].velocity, delta_time));
        particle_resolve_collisions(&particles[i]);
        // ivec3s cell = get_grid_cell(particles[i].position);
        // i32 seed = (289343 + cell.x) * 247651 + (289343 + cell.y) * 92863 + (289343 + cell.z) *
        // 452377; particles[i].colour = (vec3s){
        //     seed % 29363 / 29363.0,
        //     seed % 362237 / 362237.0,
        //     seed % 580093 / 580093.0,
        // };
    }
}

int main() {
    GLFWwindow *window = create_window();
    context render_context = context_new(window);

    pipeline_builder pipeline_builder = pipeline_builder_new(&render_context);
    pipeline_builder_set_topology(&pipeline_builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_ubo_size(&pipeline_builder, sizeof(UniformBufferObject));
    pipeline_builder_add_input_binding(&pipeline_builder,
                                       0,
                                       sizeof(vec3s),
                                       VK_VERTEX_INPUT_RATE_VERTEX);
    pipeline_builder_add_input_attribute(&pipeline_builder, 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
    pipeline_builder_add_input_binding(&pipeline_builder,
                                       1,
                                       sizeof(InstanceData),
                                       VK_VERTEX_INPUT_RATE_INSTANCE);
    pipeline_builder_add_input_attribute(&pipeline_builder,
                                         1,
                                         1,
                                         VK_FORMAT_R32G32B32A32_SFLOAT,
                                         offsetof(InstanceData, model.col[0]));
    pipeline_builder_add_input_attribute(&pipeline_builder,
                                         1,
                                         2,
                                         VK_FORMAT_R32G32B32A32_SFLOAT,
                                         offsetof(InstanceData, model.col[1]));
    pipeline_builder_add_input_attribute(&pipeline_builder,
                                         1,
                                         3,
                                         VK_FORMAT_R32G32B32A32_SFLOAT,
                                         offsetof(InstanceData, model.col[2]));
    pipeline_builder_add_input_attribute(&pipeline_builder,
                                         1,
                                         4,
                                         VK_FORMAT_R32G32B32A32_SFLOAT,
                                         offsetof(InstanceData, model.col[3]));
    pipeline_builder_add_input_attribute(&pipeline_builder,
                                         1,
                                         5,
                                         VK_FORMAT_R32G32B32_SFLOAT,
                                         offsetof(InstanceData, colour));
    pipeline_builder_set_shaders(&pipeline_builder,
                                 "shaders/simple_instanced.vert.spv",
                                 "shaders/simple_instanced.frag.spv");

    pipeline pipeline = pipeline_builder_build(&pipeline_builder, render_context.render_pass);

    vec3s *sphere_vertices = generate_sphere();

    VkDeviceSize vertex_buffer_size = sizeof(vec3s) * darray_length(sphere_vertices);

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

    memcpy(vertex_staging_buffer_memory_mapped, sphere_vertices, vertex_buffer_size);

    copy_buffer(&render_context, vertex_staging_buffer, vertex_buffer, vertex_buffer_size);

    context_begin_main_loop(&render_context);

    Camera camera = {
        {0.0, 0.0, 20.0},
        {0.0, 0.0, -1.0},
        {0.0, 1.0, 0.0},
    };

    f32 delta_time;
    f32 last_time;
    f32 last_second = glfwGetTime();
    u16 frames = 0;

    i32 prev_instance_count = -1;
    VkBuffer instance_staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory instance_staging_buffer_memory = VK_NULL_HANDLE;
    VkBuffer instance_buffer = VK_NULL_HANDLE;
    VkDeviceMemory instance_buffer_memory = VK_NULL_HANDLE;
    void *instance_staging_buffer_memory_mapped = NULL;

    Particle *particles = darray_create(Particle);

    i32 particles_per_row = (i32)powf(PARTICLE_COUNT, 1.0f / 3.0f);
    f32 spacing = PARTICLE_SIZE * 2 + PARTICLE_SPACING;

    for (i32 i = 0; i < PARTICLE_COUNT; i++) {
        float x = (i % particles_per_row - particles_per_row / 2.0f + 0.5f) * spacing;
        float y =
            (i / particles_per_row % particles_per_row - particles_per_row / 2.0f + 0.5f) * spacing;
        float z =
            ((f32)i / (particles_per_row * particles_per_row) - particles_per_row / 2.0f + 0.5f) *
            spacing;

        Particle p = {.position = {x, y, z}};
        particle_resolve_collisions(&p);
        darray_push(particles, p);
    }

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

        vec3s direction = {
            cos(glm_rad(yaw)) * cos(glm_rad(pitch)),
            sin(glm_rad(pitch)),
            sin(glm_rad(yaw)) * cos(glm_rad(pitch)),
        };
        camera.front = glms_vec3_normalize(direction);

        UniformBufferObject ubo = {
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

        memcpy(pipeline.uniform_buffer_mapped, &ubo, sizeof(ubo));

        particles_update(particles, delta_time);

        InstanceData instance_data[darray_length(particles)];
        u64 instance_data_size = sizeof(instance_data);
        for (i32 i = 0; i < darray_length(particles); i++) {
            instance_data[i].model = glms_mat4_identity();
            instance_data[i].model = glms_translate(instance_data[i].model, particles[i].position);
            instance_data[i].model =
                glms_scale(instance_data[i].model,
                           (vec3s){PARTICLE_SIZE, PARTICLE_SIZE, PARTICLE_SIZE});
            instance_data[i].colour = particles[i].colour;
        }

        if (darray_length(particles) != prev_instance_count || prev_instance_count == -1) {
            prev_instance_count = darray_length(particles);

            if (instance_buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(render_context.device.logical_device,
                                instance_staging_buffer,
                                NULL);
                vkFreeMemory(render_context.device.logical_device,
                             instance_staging_buffer_memory,
                             NULL);

                vkDestroyBuffer(render_context.device.logical_device, instance_buffer, NULL);
                vkFreeMemory(render_context.device.logical_device, instance_buffer_memory, NULL);

                instance_staging_buffer = VK_NULL_HANDLE;
                instance_staging_buffer_memory = VK_NULL_HANDLE;
                instance_buffer = VK_NULL_HANDLE;
                instance_buffer_memory = VK_NULL_HANDLE;
            }

            context_create_buffer(&render_context,
                                  instance_data_size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  &instance_staging_buffer,
                                  &instance_staging_buffer_memory);
            vkMapMemory(render_context.device.logical_device,
                        instance_staging_buffer_memory,
                        0,
                        instance_data_size,
                        0,
                        &instance_staging_buffer_memory_mapped);

            context_create_buffer(&render_context,
                                  instance_data_size,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  &instance_buffer,
                                  &instance_buffer_memory);
        }

        memcpy(instance_staging_buffer_memory_mapped, instance_data, instance_data_size);

        copy_buffer(&render_context, instance_staging_buffer, instance_buffer, instance_data_size);

        VkCommandBuffer command_buffer = context_begin_frame(&render_context);

        pipeline_bind(&pipeline, command_buffer, render_context.current_frame);

        VkBuffer buffers[] = {vertex_buffer, instance_buffer};
        VkDeviceSize offsets[] = {0, 0};
        vkCmdBindVertexBuffers(command_buffer, 0, 2, buffers, offsets);

        vkCmdDraw(command_buffer, darray_length(sphere_vertices), darray_length(particles), 0, 0);

        context_end_frame(&render_context);
    }

    context_end_main_loop(&render_context);

    vkDestroyBuffer(render_context.device.logical_device, instance_staging_buffer, NULL);
    vkFreeMemory(render_context.device.logical_device, instance_staging_buffer_memory, NULL);

    vkDestroyBuffer(render_context.device.logical_device, instance_buffer, NULL);
    vkFreeMemory(render_context.device.logical_device, instance_buffer_memory, NULL);

    vkDestroyBuffer(render_context.device.logical_device, vertex_staging_buffer, NULL);
    vkFreeMemory(render_context.device.logical_device, vertex_staging_buffer_memory, NULL);

    vkDestroyBuffer(render_context.device.logical_device, vertex_buffer, NULL);
    vkFreeMemory(render_context.device.logical_device, vertex_buffer_memory, NULL);

    pipeline_destroy(&pipeline, &render_context.device);
    context_cleanup(&render_context);
}

static vec3s *generate_sphere() {
    vec3s *result = darray_create(vec3s);

    i32 res = 12;

    for (i32 i = 0; i < res + 2; i++) {
        for (i32 j = 0; j < res; j++) {
            vec3s v;

            v = (vec3s){cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * i)) *
                            sinf(DEG2RAD * (360.0f * j / res)),
                        sinf(DEG2RAD * (270 + (180.0f / (res + 1)) * i)),
                        cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * i)) *
                            cosf(DEG2RAD * (360.0f * j / res))};
            darray_push(result, v);
            v = (vec3s){cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))) *
                            sinf(DEG2RAD * (360.0f * (j + 1) / res)),
                        sinf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))),
                        cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))) *
                            cosf(DEG2RAD * (360.0f * (j + 1) / res))};
            darray_push(result, v);
            v = (vec3s){cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))) *
                            sinf(DEG2RAD * (360.0f * j / res)),
                        sinf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))),
                        cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))) *
                            cosf(DEG2RAD * (360.0f * j / res))};
            darray_push(result, v);

            v = (vec3s){cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * i)) *
                            sinf(DEG2RAD * (360.0f * j / res)),
                        sinf(DEG2RAD * (270 + (180.0f / (res + 1)) * i)),
                        cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * i)) *
                            cosf(DEG2RAD * (360.0f * j / res))};
            darray_push(result, v);
            v = (vec3s){cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i))) *
                            sinf(DEG2RAD * (360.0f * (j + 1) / res)),
                        sinf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i))),
                        cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i))) *
                            cosf(DEG2RAD * (360.0f * (j + 1) / res))};
            darray_push(result, v);
            v = (vec3s){cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))) *
                            sinf(DEG2RAD * (360.0f * (j + 1) / res)),
                        sinf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))),
                        cosf(DEG2RAD * (270 + (180.0f / (res + 1)) * (i + 1))) *
                            cosf(DEG2RAD * (360.0f * (j + 1) / res))};
            darray_push(result, v);
        }
    }

    return result;
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
