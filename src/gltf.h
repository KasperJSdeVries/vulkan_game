#ifndef GLFT_H
#define GLFT_H

#include <vm/vm.h>

#include <stdio.h>
#include <string.h>

typedef struct {
    u32 version_major;
    u32 version_minor;
} gltf_asset;

typedef struct {
    u32 node_count;
    u64 *nodes;
} gltf_scene;

typedef struct {
    mat4 matrix;
    vec4 rotation;
    vec3 scale;
    vec3 translation;
    u64 mesh;
} gltf_node;

typedef enum {
    GLTF_MESH_ATTRIBUTE_POSITION,
    GLTF_MESH_ATTRIBUTE_NORMAL,
} gltf_mesh_attribute_type;

typedef enum {
    GLTF_MESH_MODE_POINTS = 0,
    GLTF_MESH_MODE_LINES = 1,
    GLTF_MESH_MODE_LINE_LOOP = 2,
    GLTF_MESH_MODE_LINE_STRIP = 3,
    GLTF_MESH_MODE_TRIANGLES = 4,
    GLTF_MESH_MODE_TRIANGLE_STRIP = 5,
    GLTF_MESH_MODE_TRIANGLE_FAN = 6,
} gltf_mesh_mode;

typedef struct {
    i64 indices_accessor_index;
    struct {
        gltf_mesh_attribute_type type;
        u64 index;
    } *attributes;
    u32 attribute_count;
    gltf_mesh_mode mode;
} gltf_mesh_primitive;

typedef struct {
    gltf_mesh_primitive *primitives;
    u32 primitive_count;
} gltf_mesh;

typedef enum {
    GLTF_ACCESSOR_COMPONENT_TYPE_BYTE = 5120,
    GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_BYTE = 5121,
    GLTF_ACCESSOR_COMPONENT_TYPE_SHORT = 5122,
    GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_SHORT = 5123,
    GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_INT = 5125,
    GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT = 5126,
} gltf_accessor_component_type;

#define ENUMERATE_ACCESSOR_TYPES                                                                   \
    __ENUMERATE_ACCESSOR_TYPE(SCALAR)                                                              \
    __ENUMERATE_ACCESSOR_TYPE(VEC2)                                                                \
    __ENUMERATE_ACCESSOR_TYPE(VEC3)                                                                \
    __ENUMERATE_ACCESSOR_TYPE(VEC4)                                                                \
    __ENUMERATE_ACCESSOR_TYPE(MAT2)                                                                \
    __ENUMERATE_ACCESSOR_TYPE(MAT3)                                                                \
    __ENUMERATE_ACCESSOR_TYPE(MAT4)

typedef enum {
#define __ENUMERATE_ACCESSOR_TYPE(type) GLTF_ACCESSOR_TYPE_##type,
    ENUMERATE_ACCESSOR_TYPES
#undef __ENUMERATE_ACCESSOR_TYPE
} gltf_accessor_type;

static gltf_accessor_type _accessor_type_from_string(char *string, u32 string_length) {
#define __ENUMERATE_ACCESSOR_TYPE(type)                                                            \
    if (strncmp(#type, string, string_length) == 0) {                                              \
        return GLTF_ACCESSOR_TYPE_##type;                                                          \
    }
    ENUMERATE_ACCESSOR_TYPES
#undef __ENUMERATE_ACCESSOR_TYPE

    // FIXME: Shouldn't be possible if using correctly generated glTF
    // but we should handle the error more gracefully
    fprintf(stderr, "Unknown accessor type: %.*s", string_length, string);
    return GLTF_ACCESSOR_TYPE_VEC3;
}

#undef ENUMERATE_ACCESSOR_TYPES

typedef struct {
    union {
        f32 scalar;
        vec2 vec2;
        vec3 vec3;
        vec4 vec4;
        mat2 mat2;
        mat3 mat3;
        mat4 mat4;
        f32 raw[16];
    } max, min;
    u64 byte_offset;
    u64 count;
    u32 buffer_view;
    gltf_accessor_component_type component_type;
    gltf_accessor_type type;
    b8 normalized;
} gltf_accessor;

typedef enum {
    GLTF_BUFFER_VIEW_TARGET_UNDEFINED,
    GLTF_BUFFER_VIEW_TARGET_ARRAY_BUFFER = 34962,
    GLTF_BUFFER_VIEW_TARGET_ELEMENT_ARRAY_BUFFER = 34963,
} gltf_buffer_view_target;

typedef struct {
    u64 byte_offset;
    u64 byte_length;
    u32 buffer;
    gltf_buffer_view_target target;
    i16 byte_stride; // -1 = not defined
} gltf_buffer_view;

typedef struct {
    u64 byte_length;
} gltf_buffer;

typedef struct {
    gltf_asset asset;
    gltf_scene default_scene;
    gltf_node *nodes;
    gltf_mesh *meshes;
    gltf_accessor *accessors;
    gltf_buffer_view *buffer_views;
    gltf_buffer *buffers;
    void **buffer_data;
    u32 scene;
    u32 node_count;
    u32 mesh_count;
    u32 accessor_count;
    u32 buffer_view_count;
    u32 buffer_count;
} gltf_root;

void load_gltf_from_file(const char *file_name, gltf_root *out_gltf);

#endif // GLFT_H
