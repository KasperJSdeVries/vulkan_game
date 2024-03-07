#include "application.h"
#include "darray.h"
#include "defines.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t vertex_count;
    vec3s *vertices;
    uint32_t *indices;
} Mesh;

typedef struct {
    Mesh mesh;
    int resolution;
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
    free(terrain_face->mesh.vertices);
    free(terrain_face->mesh.indices);

    terrain_face->mesh.vertex_count =
        terrain_face->resolution * terrain_face->resolution;

    terrain_face->mesh.vertices =
        malloc(terrain_face->mesh.vertex_count * sizeof(vec3s));

    size_t triangles_length =
        (terrain_face->resolution - 1) * (terrain_face->resolution - 1) * 6;

    terrain_face->mesh.indices = malloc(triangles_length * sizeof(uint32_t));

    int triangle_index = 0;
    for (int y = 0; y < terrain_face->resolution; y++) {
        for (int x = 0; x < terrain_face->resolution; x++) {
            int i = x + y * terrain_face->resolution;
            vec2s percent =
                glms_vec2_divs((vec2s){x, y}, (terrain_face->resolution - 1));
            // local_up
            // + (percent.x - 0.5f) * 2.0f * axis_a
            // + (percent.y - 0.5f) * 2.0f * axis_b
            vec3s point_on_unit_cube = glms_vec3_add(
                glms_vec3_add(terrain_face->local_up,
                              glms_vec3_scale(terrain_face->axis_a,
                                              (percent.x - 0.5f) * 2.0f)),
                glms_vec3_scale(terrain_face->axis_b,
                                (percent.y - 0.5f) * 2.0f));
            terrain_face->mesh.vertices[i] = point_on_unit_cube;

            if (x != terrain_face->resolution - 1 &&
                y != terrain_face->resolution - 1) {
                terrain_face->mesh.indices[triangle_index] = i;
                terrain_face->mesh.indices[triangle_index + 1] =
                    i + terrain_face->resolution + 1;
                terrain_face->mesh.indices[triangle_index + 2] =
                    i + terrain_face->resolution;

                terrain_face->mesh.indices[triangle_index + 3] = i;
                terrain_face->mesh.indices[triangle_index + 4] = i + 1;
                terrain_face->mesh.indices[triangle_index + 5] = i =
                    terrain_face->resolution + 1;
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
    Planet planet;
    int resolution = 10;
    vec3s directions[] = {
        {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
    };

    for (int i = 0; i < FACES_PER_PLANET; i++) {
        planet.terrain_faces[i] =
            create_terrain_face(resolution, directions[i]);
    }

    return planet;
};

static void planet_generate_meshes(Planet *planet) {
    for (int i = 0; i < FACES_PER_PLANET; i++) {
        terrain_face_construct_mesh(&planet->terrain_faces[i]);
    }
};

int main() {
    Application app = {0};

    init_window(&app);
    init_vulkan(&app);

    main_loop(&app);

    cleanup(&app);
}
