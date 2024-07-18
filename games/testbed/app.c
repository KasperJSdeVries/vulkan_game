#include "system.h"
#include "text_system.h"
#include <application.h>
#include <containers/darray.h>
#include <defines.h>
#include <font/font.h>
#include <parsers/gltf.h>
#include <renderer/camera.h>
#include <renderer/context.h>
#include <renderer/pipeline.h>
#include <renderer/types.h>
#include <window.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1280
#define HEIGHT 720

#ifdef WIN32
#define MODULE_API __declspec(dllexport)
#else
#define MODULE_API
#endif

#define APP_API(name, ret, ...) MODULE_API ret name(__VA_ARGS__);
LIST_OF_APP_APIS
#undef APP_API

static struct state {
} *s;

typedef struct {
    vec2s aa;
    vec2s bb;
    vec3s color;
} ColoredRectangle;

typedef struct {
    u32 vertex_count;
    vec3s *vertices;
    u32 *indices;
    u64 index_count;
} Mesh;

typedef struct {
    Mesh mesh;
    i32 resolution;
    vec3s local_up;
    vec3s axis_a;
    vec3s axis_b;
} TerrainFace;

#define FACES_PER_PLANET 6

typedef struct {
    TerrainFace terrain_faces[FACES_PER_PLANET];
} Planet;

static TerrainFace create_terrain_face(int resolution, vec3s local_up) {
    TerrainFace terrain_face;
    terrain_face.resolution = resolution;
    terrain_face.local_up = local_up;
    terrain_face.axis_a = (vec3s){{local_up.y, local_up.z, local_up.x}};
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

    terrain_face->mesh.index_count =
        (terrain_face->resolution - 1) * (terrain_face->resolution - 1) * 6;

    terrain_face->mesh.indices = malloc(terrain_face->mesh.index_count * sizeof(u32));

    u32 triangle_index = 0;
    for (int y = 0; y < terrain_face->resolution; y++) {
        for (int x = 0; x < terrain_face->resolution; x++) {
            u32 i = x + y * terrain_face->resolution;
            vec2s percent = glms_vec2_divs((vec2s){{x, y}}, (terrain_face->resolution - 1));
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

static Planet create_planet(void) {
    Planet planet = {0};
    int resolution = 50;
    vec3s directions[] = {
        {{0, 0, 1}},
        {{0, 0, -1}},
        {{0, 1, 0}},
        {{0, -1, 0}},
        {{1, 0, 0}},
        {{-1, 0, 0}},
    };

    for (int i = 0; i < FACES_PER_PLANET; i++) {
        planet.terrain_faces[i] = create_terrain_face(resolution, directions[i]);
    }

    return planet;
}

static void planet_generate_meshes(Planet *planet) {
    for (int i = 0; i < FACES_PER_PLANET; i++) {
        terrain_face_construct_mesh(&planet->terrain_faces[i]);
    }
}

typedef struct {
    pipeline rectangle_pipeline;
    ColoredRectangle *rectangles; // darray

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer instance_buffer;
    VkDeviceMemory instance_buffer_memory;
} ColoredRectangleRenderer;

static ColoredRectangleRenderer colored_rectangle_renderer_create(context *render_context) {
    pipeline_builder ui_pipeline_builder = pipeline_builder_new(render_context);
    pipeline_builder_set_shaders(&ui_pipeline_builder,
                                 "shaders/ui.vert.spv",
                                 "shaders/ui.frag.spv");
    pipeline_builder_add_input_binding(&ui_pipeline_builder,
                                       0,
                                       sizeof(vec2s) * 2,
                                       VK_VERTEX_INPUT_RATE_VERTEX);
    pipeline_builder_add_input_attribute(&ui_pipeline_builder, 0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
    pipeline_builder_add_input_attribute(&ui_pipeline_builder,
                                         0,
                                         1,
                                         VK_FORMAT_R32G32_SFLOAT,
                                         sizeof(vec2s));
    pipeline_builder_add_input_binding(&ui_pipeline_builder,
                                       1,
                                       sizeof(vec3s),
                                       VK_VERTEX_INPUT_RATE_INSTANCE);
    pipeline_builder_add_input_attribute(&ui_pipeline_builder, 1, 2, VK_FORMAT_R32G32B32_SFLOAT, 0);
    pipeline_builder_set_topology(&ui_pipeline_builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    pipeline_builder_set_alpha_blending(&ui_pipeline_builder, true);

    return (ColoredRectangleRenderer){
        .rectangles = darray_create(ColoredRectangle),
        .rectangle_pipeline =
            pipeline_builder_build(&ui_pipeline_builder, render_context->render_pass),
    };
}

static void colored_rectangle_renderer_destroy(ColoredRectangleRenderer *renderer,
                                               device *render_device) {
    darray_destroy(renderer->rectangles);

    vkDestroyBuffer(render_device->logical_device, renderer->vertex_buffer, NULL);
    vkFreeMemory(render_device->logical_device, renderer->vertex_buffer_memory, NULL);

    vkDestroyBuffer(render_device->logical_device, renderer->instance_buffer, NULL);
    vkFreeMemory(render_device->logical_device, renderer->instance_buffer_memory, NULL);

    pipeline_destroy(&renderer->rectangle_pipeline, render_device);
}

static void colored_rectangle_renderer_add_rectangle(ColoredRectangleRenderer *renderer,
                                                     vec2s aa,
                                                     vec2s bb,
                                                     vec3s color) {
    ColoredRectangle rect = {aa, bb, color};
    darray_push(renderer->rectangles, rect);
}

static void colored_rectangle_renderer_setup_buffers(ColoredRectangleRenderer *renderer,
                                                     context *render_context) {
    VkDeviceSize vertex_buffer_size = sizeof(vec2s) * 4 * 2 * darray_length(renderer->rectangles);
    VkDeviceSize instance_buffer_size = sizeof(vec3s) * darray_length(renderer->rectangles);

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

    for (u32 i = 0; i < darray_length(renderer->rectangles); i++) {
        vec2s buf[] = {
            {{renderer->rectangles[i].aa.x, renderer->rectangles[i].aa.y}},
            {{0, 0}},
            {{renderer->rectangles[i].aa.x, renderer->rectangles[i].bb.y}},
            {{0, 1}},
            {{renderer->rectangles[i].bb.x, renderer->rectangles[i].aa.y}},
            {{1, 0}},
            {{renderer->rectangles[i].bb.x, renderer->rectangles[i].bb.y}},
            {{1, 1}},
        };

        memcpy((void *)((u64)vertex_staging_buffer_memory_mapped + i * sizeof(vec2s)),
               buf,
               sizeof(buf));
    }

    context_copy_buffer(render_context,
                        vertex_staging_buffer,
                        renderer->vertex_buffer,
                        vertex_buffer_size);

    vkDestroyBuffer(render_context->device.logical_device, vertex_staging_buffer, NULL);
    vkFreeMemory(render_context->device.logical_device, vertex_staging_buffer_memory, NULL);

    VkBuffer instance_staging_buffer;
    VkDeviceMemory instance_staging_buffer_memory;

    context_create_buffer(render_context,
                          instance_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &instance_staging_buffer,
                          &instance_staging_buffer_memory);

    context_create_buffer(render_context,
                          instance_buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &renderer->instance_buffer,
                          &renderer->instance_buffer_memory);

    void *instance_staging_buffer_memory_mapped;
    vkMapMemory(render_context->device.logical_device,
                instance_staging_buffer_memory,
                0,
                instance_buffer_size,
                0,
                &instance_staging_buffer_memory_mapped);

    vec3s colors[darray_length(renderer->rectangles)];
    for (u32 i = 0; i < darray_length(renderer->rectangles); i++) {
        colors[i] = renderer->rectangles[i].color;
    }

    memcpy(instance_staging_buffer_memory_mapped, colors, sizeof(colors));
    context_copy_buffer(render_context,
                        instance_staging_buffer,
                        renderer->instance_buffer,
                        instance_buffer_size);

    vkDestroyBuffer(render_context->device.logical_device, instance_staging_buffer, NULL);
    vkFreeMemory(render_context->device.logical_device, instance_staging_buffer_memory, NULL);
}

static void colored_rectangle_renderer_render(ColoredRectangleRenderer *renderer,
                                              u32 current_frame,
                                              VkCommandBuffer command_buffer) {
    pipeline_bind(&renderer->rectangle_pipeline, command_buffer, current_frame);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &renderer->vertex_buffer, offsets);
    vkCmdBindVertexBuffers(command_buffer, 1, 1, &renderer->instance_buffer, offsets);

    vkCmdDraw(command_buffer, 4, darray_length(renderer->rectangles), 0, 0);
}

vec4s calculate_plane(vec3s vertices[3]) {
    vec3s ab = glms_vec3_sub(vertices[1], vertices[0]);
    vec3s ac = glms_vec3_sub(vertices[2], vertices[0]);
    vec3s normal = glms_normalize(glms_vec3_cross(ab, ac));
    float d = -glms_vec3_dot(normal, vertices[0]);
    return (vec4s){{normal.x, normal.y, normal.z, d}};
}

mat4s glms_mat4_add(mat4s m1, mat4s m2) {
    mat4s dest = {0};

    for (u8 i = 0; i < 4; i++)
        for (u8 j = 0; j < 4; j++)
            dest.raw[i][j] = m1.raw[i][j] + m2.raw[i][j];

    return dest;
}

mat4s calculate_fundamental_error_quadric(vec4s plane) {
    mat4s quadratic = {0};
    for (u8 i = 0; i < 4; i++)
        for (u8 j = 0; j < 4; j++)
            quadratic.raw[i][j] = plane.raw[i] * plane.raw[j];
    return quadratic;
}

void memswap(void *a, void *b, u64 size) {
    if (a == b || size == 0) {
        return;
    }

    u8 buffer[size];
    memcpy(buffer, a, size);
    memcpy(a, b, size);
    memcpy(b, buffer, size);
}

f32 calculate_cost(mat4s approximate_error, vec3s point) {
    vec4s v = {{point.x, point.y, point.z, 1.0f}};
    vec4s F = glms_mat4_mulv(approximate_error, v);
    return glms_vec4_dot(v, F);
}

#define SWAP(a, b)                                                                                 \
    {                                                                                              \
        typeof(b) temp = a;                                                                        \
        a = b;                                                                                     \
        b = temp;                                                                                  \
    }

struct contraction_target {
    u32 pair[2];
    vec3s contracted_vertex;
    f32 cost;
};

struct contraction_target target_create(const Mesh *mesh,
                                        const mat4s *error_quadrics,
                                        const u32 pair[2]) {
    struct contraction_target new_target = {.pair[0] = pair[0], .pair[1] = pair[1]};

    mat4s Q = glms_mat4_add(error_quadrics[pair[0]], error_quadrics[pair[1]]);

    mat4s minimum_error_matrix = {
        .col =
            {
                {{Q.m00, Q.m01, Q.m02, 0.0}},
                {{Q.m01, Q.m11, Q.m12, 0.0}},
                {{Q.m02, Q.m12, Q.m22, 0.0}},
                {{Q.m03, Q.m13, Q.m23, 1.0}},
            },
    };

    // Check if the matrix is invertible
    if (fabsf(glms_mat4_det(minimum_error_matrix)) > 1e-6) {
        minimum_error_matrix = glms_mat4_inv(minimum_error_matrix);

        vec4s result = glms_mat4_mulv(minimum_error_matrix, (vec4s){{0, 0, 0, 1}});

        new_target.contracted_vertex = glms_vec3(result);
        new_target.cost = calculate_cost(Q, new_target.contracted_vertex);
    } else {
        vec3s new_positions[] = {
            mesh->vertices[pair[0]],
            mesh->vertices[pair[1]],
            glms_vec3_scale(glms_vec3_add(mesh->vertices[pair[0]], mesh->vertices[pair[1]]), 0.5),
        };

        new_target.cost = F32_MAX;
        new_target.contracted_vertex = mesh->vertices[pair[0]];

        for (u32 j = 0; j < 3; j++) {
            f32 cost = calculate_cost(Q, new_positions[j]);
            if (cost < new_target.cost) {
                new_target.contracted_vertex = new_positions[j];
                new_target.cost = cost;
            }
        }
    }

    return new_target;
}

void simplify_mesh(Mesh *mesh, f32 error_limit) {
    // Initialize error quadrics for each vertex.
    mat4s error_quadrics[mesh->vertex_count];
    memset(error_quadrics, 0, sizeof(error_quadrics));

    // Create dynamic array for storing vertex pairs.
    u32(*pairs)[2] = darray_create(u32[2]);

    // Compute error quadrics for each triangle and identify unique vertex pairs.
    for (u32 i = 0; i < mesh->index_count; i += 3) {
        vec3s triangle[3] = {
            mesh->vertices[mesh->indices[i]],
            mesh->vertices[mesh->indices[i + 1]],
            mesh->vertices[mesh->indices[i + 2]],
        };
        vec4s p = calculate_plane(triangle);
        mat4s Kp = calculate_fundamental_error_quadric(p);
        for (u32 j = 0; j < 3; j++) {
            // Accumulate error quadric for each vertex of the triangle.
            error_quadrics[mesh->indices[i + j]] =
                glms_mat4_add(error_quadrics[mesh->indices[i + j]], Kp);

            // Create a pair of adjacent vertices.
            u32 pair[2] = {mesh->indices[i + j], mesh->indices[i + ((j + 1) % 3)]};
            if (pair[0] > pair[1]) {
                SWAP(pair[0], pair[1]);
            }

            // Check if the pair is already known.
            b8 already_known = false;
            for (u64 k = 0; k < darray_length(pairs); k++) {
                if (pair[0] == pairs[k][0] && pair[1] == pairs[k][1]) {
                    already_known = true;
                    break;
                }
            }
            // Add the pair if it is unique.
            if (!already_known) {
                pairs = _darray_push(pairs, &pair);
            }
        }
    }

    // Create contraction targets for each pair.
    struct contraction_target targets[darray_length(pairs)];
    for (u64 i = 0; i < darray_length(pairs); i++) {
        targets[i] = target_create(mesh, error_quadrics, pairs[i]);

        // Maintain min-heap property based on cost.
        u64 new_index = i;
        while (new_index > 0 && targets[(new_index - 1) / 2].cost > targets[new_index].cost) {
            memswap(&targets[(new_index - 1) / 2], &targets[new_index], sizeof(targets[0]));
            new_index = (new_index - 1) / 2;
        }
    }

    u32 vertices_merged = 0;

    // Simplify mesh by contracting vertex pairs with the lowest cost.
    u64 targets_top = darray_length(pairs) - 1;
    while (targets[0].cost < error_limit && targets_top > 0) {
        struct contraction_target target = targets[0];
        for (u64 i = 0; i < darray_length(pairs); i++) {
            if (pairs[i][0] == target.pair[0] && pairs[i][1] == target.pair[1]) {
                darray_pop_at(pairs, i, NULL);
            }
        }

        // Replace the top of the heap with the last element.
        targets[0] = targets[targets_top--];

        // Restore heap property.
        u64 i = 0;
        while (i <= targets_top) {
            u64 smallest = i;
            u64 left = 2 * i + 1;
            u64 right = 2 * i + 2;

            if (left <= targets_top && targets[left].cost < targets[smallest].cost) {
                smallest = left;
            }
            if (right <= targets_top && targets[right].cost < targets[smallest].cost) {
                smallest = right;
            }

            if (smallest != i) {
                memswap(&targets[i], &targets[smallest], sizeof(targets[0]));
                i = smallest;
            } else {
                break;
            }
        }

        // Output the vertices being merged.
        vertices_merged++;
        // printf("Attempting to merge vertices: %d & %d\n", target.pair[0], target.pair[1]);

        // Contract the vertices.
        mesh->vertices[target.pair[0]] = target.contracted_vertex;
        if (target.pair[1] != mesh->vertex_count - 1) {
            mesh->vertices[target.pair[1]] = mesh->vertices[mesh->vertex_count - 1];
            error_quadrics[target.pair[1]] = error_quadrics[mesh->vertex_count - 1];
        }
        mesh->vertex_count--;

        // Update indices to reflect the contraction.
        for (u32 i = 0; i < mesh->index_count; i++) {
            if (mesh->indices[i] == mesh->vertex_count) {
                mesh->indices[i] = target.pair[1];
            }
            if (mesh->indices[i] == target.pair[1]) {
                mesh->indices[i] = target.pair[0];
            }
        }

        // Recompute error quadrics for the contracted vertex.
        mat4s new_quadratics = error_quadrics[target.pair[0]];
        for (u32 j = 0; j < mesh->index_count; j += 3) {
            if (mesh->indices[j] == target.pair[0] || mesh->indices[j + 1] == target.pair[0] ||
                mesh->indices[j + 2] == target.pair[0]) {
                vec3s triangle[3] = {
                    mesh->vertices[mesh->indices[j]],
                    mesh->vertices[mesh->indices[j + 1]],
                    mesh->vertices[mesh->indices[j + 2]],
                };
                vec4s p = calculate_plane(triangle);
                mat4s Kp = calculate_fundamental_error_quadric(p);
                new_quadratics = glms_mat4_add(new_quadratics, Kp);
            }
        }
        error_quadrics[target.pair[0]] = new_quadratics;

        // Update contraction targets for affected pairs.
        for (u32 j = 0; j < darray_length(pairs); j++) {
            if (pairs[j][0] == target.pair[0] || pairs[j][1] == target.pair[0]) {
                targets[j] = target_create(mesh, error_quadrics, pairs[j]);

                // Restore heap property.
                u64 child_index = j;
                while (child_index > 0) {
                    u64 parent_index = (child_index - 1) / 2;
                    if (targets[parent_index].cost > targets[child_index].cost) {
                        memswap(&targets[parent_index], &targets[child_index], sizeof(targets[0]));
                        child_index = parent_index;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    printf("merged %d vertices\n", vertices_merged);

    // Clean up dynamic arrays.
    darray_destroy(pairs);
}

/*
int not_main(void) {
    gltf_root gltf;
    load_gltf_from_file("models/tire.glb", &gltf);

    struct font font;
    load_font("fonts/foxus/FOXUS.ttf", &font);
    // load_font("fonts/unispace/Unispace Rg.otf", &font);

    // ColoredRectangleRenderer rectangle_renderer =
    // colored_rectangle_renderer_create(&render_context);

    // colored_rectangle_renderer_add_rectangle(&rectangle_renderer, (vec2s){{0.1, 0.1}},
    // (vec2s){{0.2, 0.2}}, (vec3s){{1.0, 0.0, 0.0}});

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

    // for (u32 i = 0; i < 6; i++) {
    //     simplify_mesh(&planet.terrain_faces[i].mesh, 0.25);
    // }

    VkDeviceSize vertex_buffer_size = sizeof(vec3s) * (planet.terrain_faces[0].mesh.vertex_count);

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

        memcpy(vertex_staging_buffer_memory_mapped,
               planet.terrain_faces[i].mesh.vertices,
               vertex_buffer_size);

        context_copy_buffer(&render_context,
                            vertex_staging_buffer,
                            vertex_buffers[i],
                            vertex_buffer_size);
    }

    VkDeviceSize index_buffer_size = sizeof(u32) * planet.terrain_faces[0].mesh.index_count;

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

    context_copy_buffer(&render_context, index_staging_buffer, index_buffers, index_buffer_size);

    // colored_rectangle_renderer_setup_buffers(&rectangle_renderer, &render_context);


    Camera camera = camera_create((vec3s){{0.0, 0.0, 5.0}});


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        camera_process_input(window, &camera, delta_time);


        pipeline_bind(&planet_pipeline, command_buffer, render_context.current_frame);

        vkCmdBindIndexBuffer(command_buffer, index_buffers, 0, VK_INDEX_TYPE_UINT32);

        for (u32 i = 0; i < FACES_PER_PLANET; i++) {
            VkBuffer buffers[] = {vertex_buffers[i]};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, buffers, offsets);

            vkCmdDrawIndexed(command_buffer, planet.terrain_faces[i].mesh.index_count, 1, 0, 0, 0);
        }

        // colored_rectangle_renderer_render(&rectangle_renderer, render_context.current_frame,
        // command_buffer);


        UniformBufferObject ubo = camera_create_ubo(&render_context, camera);

        memcpy(planet_pipeline.uniform_buffer_mapped, &ubo, sizeof(ubo));
    }


    // colored_rectangle_renderer_destroy(&rectangle_renderer, &render_context.device);
    text_renderer_destroy(&text_renderer, &render_context.device);

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

    return 0;
}
*/

void app_init(engine *e) {
    (void)e;
    s = malloc(sizeof(*s));
    memset(s, 0, sizeof(*s));

    struct window_create_info window_info = {
        .width = WIDTH,
        .height = HEIGHT,
    };

    window_init(e, window_info);
    window_set_mouse_pos_callback(e, camera_mouse_callback);

    system_create_info text_system_info = {
        .init = text_system_init,
        .cleanup = text_system_cleanup,
        .render = text_system_render,
    };
    engine_add_system(e, text_system_info);
}

void *app_pre_reload(void) { return s; }

void app_post_reload(void *sp) { s = sp; }

void app_cleanup(void) { free(s); }
