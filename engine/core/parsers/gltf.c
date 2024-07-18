#include "gltf.h"

#include "cglm/struct/vec3.h"
#include "cglm/types-struct.h"
#include "defines.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLTF_MAGIC 0x46546C67

struct gltf_header {
    u32 magic;
    u32 version;
    u32 length;
};

#define GLTF_CHUNK_TYPE_JSON 0x4E4F534A
#define GLTF_CHUNK_TYPE_BIN 0x004E4942

struct gltf_chunk {
    u32 chunk_length;
    u32 chunk_type;
    u8 chunk_data[];
};

static void parse_gltf(json_value *gltf, gltf_root *out_data);

void load_gltf_from_file(const char *file_name, gltf_root *out_gltf) {
    (void)out_gltf;
    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        fprintf(stderr, "Unable to open file: %s\n", file_name);
        return;
    }

    struct gltf_header header;
    if (fread(&header, sizeof(struct gltf_header), 1, fp) != 1) {
        fprintf(stderr, "unable to read file header: %s\n", file_name);
        return;
    }
    if (header.magic != GLTF_MAGIC) {
        fprintf(stderr, "%s is not a glTF file\n", file_name);
        return;
    }
    if (header.version != 2) {
        fprintf(stderr, "glTF binary version is not supported for: %s\n", file_name);
        return;
    }

    u32 buffer_index = 0;
    while (ftell(fp) < header.length) {
        struct gltf_chunk temp_chunk;
        if (fread(&temp_chunk, sizeof(temp_chunk), 1, fp) != 1) {
            fprintf(stderr, "unable to read chunk from: %s\n", file_name);
            return;
        }

        struct gltf_chunk *chunk = malloc(sizeof(*chunk) + temp_chunk.chunk_length);
        chunk->chunk_length = temp_chunk.chunk_length;
        chunk->chunk_type = temp_chunk.chunk_type;
        if (fread(chunk->chunk_data, sizeof(u8), chunk->chunk_length, fp) != chunk->chunk_length) {
            fprintf(stderr,
                    "unable to read %s chunk data from: %s\n",
                    chunk->chunk_type == GLTF_CHUNK_TYPE_JSON  ? "json"
                    : chunk->chunk_type == GLTF_CHUNK_TYPE_BIN ? "binary"
                                                               : "unknown",
                    file_name);
            return;
        }

        if (chunk->chunk_type == GLTF_CHUNK_TYPE_JSON) {
            json_value *gltf = json_parse((char *)chunk->chunk_data, chunk->chunk_length);
            if (gltf == NULL) {
                fprintf(stderr, "Failed to parse json!\n");
                return;
            }
            if (gltf->type != JSON_VALUE_OBJECT) {
                fprintf(stderr, "gltf is not an object\n");
                return;
            }
            parse_gltf(gltf, out_gltf);
            json_value_free(gltf);

            out_gltf->buffer_data = malloc(sizeof(*out_gltf->buffer_data) * out_gltf->buffer_count);
            for (u32 i = 0; i < out_gltf->buffer_count; i++) {
                out_gltf->buffer_data[i] = malloc(out_gltf->buffers[i].byte_length);
            }
        }

        if (chunk->chunk_type == GLTF_CHUNK_TYPE_BIN) {
            memcpy(out_gltf->buffer_data[buffer_index],
                   chunk->chunk_data,
                   out_gltf->buffers[buffer_index].byte_length);
            buffer_index++;
        }

        free(chunk);
    }

    fclose(fp);
}

#define json_get_value(object, value, value_type)                                                  \
    {                                                                                              \
        value = json_object_get_value(object, #value);                                             \
        if (!value) {                                                                              \
            fprintf(stderr, "gltf is missing \"%s\" key\n", #value);                               \
            return;                                                                                \
        }                                                                                          \
        if (value->type != value_type) {                                                           \
            fprintf(stderr, "%s value has an incorrect type\n", #value);                           \
            return;                                                                                \
        }                                                                                          \
    }

static void parse_asset(json_value *asset, gltf_asset *out_data) {
    json_value *version;
    json_get_value(asset, version, JSON_VALUE_STRING);
    char temp[version->u.string.length + 1];
    memcpy(temp, version->u.string.ptr, version->u.string.length + 1);
    temp[version->u.string.length] = 0;
    sscanf(temp, "%u.%u", &out_data->version_major, &out_data->version_minor);
}

static void parse_scene(json_value *scene, gltf_scene *out_data) {
    json_value *nodes;
    json_get_value(scene, nodes, JSON_VALUE_ARRAY);

    out_data->node_count = nodes->u.array.length;
    out_data->nodes = malloc(sizeof(*(out_data->nodes)) * nodes->u.array.length);
    for (u32 i = 0; i < nodes->u.array.length; i++) {
        if (nodes->u.array.values[i]->type != JSON_VALUE_INTEGER) {
            fprintf(stderr, "nodes array contains value that is not an integer\n");
            return;
        }
        out_data->nodes[i] = nodes->u.array.values[i]->u.integer;
    }
}

static void parse_node(json_value *node, gltf_node *out_data) {
    json_value *matrix = json_object_get_value(node, "matrix");
    if (matrix) {
        json_get_value(node, matrix, JSON_VALUE_ARRAY);
        // TODO: Finish implementing reading out the matrix data
    } else {
        out_data->matrix = glms_mat4_identity();
    }

    json_value *rotation = json_object_get_value(node, "rotation");
    if (rotation) {
        json_get_value(node, rotation, JSON_VALUE_ARRAY);
        // TODO: Finish implementing reading out the rotation data
    } else {
        out_data->rotation = (vec4s){{0.0f, 0.0f, 0.0f, 1.0f}};
    }

    json_value *scale = json_object_get_value(node, "scale");
    if (scale) {
        json_get_value(node, scale, JSON_VALUE_ARRAY);
        // TODO: Finish implementing reading out the scale data
    } else {
        out_data->scale = glms_vec3_one();
    }

    json_value *translation = json_object_get_value(node, "translation");
    if (translation) {
        json_get_value(node, translation, JSON_VALUE_ARRAY);
        // TODO: Finish implementing reading out the translation data
    } else {
        out_data->translation = glms_vec3_zero();
    }

    json_value *mesh;
    json_get_value(node, mesh, JSON_VALUE_INTEGER);
    out_data->mesh = mesh->u.integer;
}

static void parse_primitive(json_value *primitive, gltf_mesh_primitive *out_data) {
    json_value *attributes;
    json_get_value(primitive, attributes, JSON_VALUE_OBJECT);

    json_value *POSITION = json_object_get_value(attributes, "POSITION");
    json_value *NORMAL = json_object_get_value(attributes, "NORMAL");

    u32 attribute_count = 0;
    if (POSITION) {
        attribute_count++;
        json_get_value(attributes, POSITION, JSON_VALUE_INTEGER);
    }
    if (NORMAL) {
        attribute_count++;
        json_get_value(attributes, NORMAL, JSON_VALUE_INTEGER);
    }

    out_data->attribute_count = attribute_count;
    out_data->attributes = malloc(sizeof(*out_data->attributes) * attribute_count);
    {
        u32 i = 0;
        if (POSITION) {
            out_data->attributes[i].type = GLTF_MESH_ATTRIBUTE_POSITION;
            out_data->attributes[i].index = POSITION->u.integer;
            i++;
        }
        if (NORMAL) {
            out_data->attributes[i].type = GLTF_MESH_ATTRIBUTE_NORMAL;
            out_data->attributes[i].index = NORMAL->u.integer;
            i++;
        }
    }

    out_data->indices_accessor_index = -1;
    if (json_object_get_value(primitive, "indices")) {
        json_value *indices;
        json_get_value(primitive, indices, JSON_VALUE_INTEGER);
        out_data->indices_accessor_index = indices->u.integer;
    }

    out_data->mode = GLTF_MESH_MODE_TRIANGLES;
    if (json_object_get_value(primitive, "mode")) {
        json_value *mode;
        json_get_value(primitive, mode, JSON_VALUE_INTEGER);
        out_data->mode = mode->u.integer;
    }
}

static void parse_mesh(json_value *mesh, gltf_mesh *out_data) {
    json_value *primitives;
    json_get_value(mesh, primitives, JSON_VALUE_ARRAY);

    out_data->primitive_count = primitives->u.array.length;
    out_data->primitives = malloc(sizeof(*out_data->primitives) * primitives->u.array.length);
    for (u32 i = 0; i < primitives->u.array.length; i++) {
        if (primitives->u.array.values[i]->type != JSON_VALUE_OBJECT) {
            fprintf(stderr, "value at index %d in nodes array is not an object", i);
            return;
        }
        parse_primitive(primitives->u.array.values[i], &out_data->primitives[i]);
    }
}

static void parse_accessor(json_value *accessor, gltf_accessor *out_data) {
    json_value *bufferView;
    json_get_value(accessor, bufferView, JSON_VALUE_INTEGER);
    out_data->buffer_view = bufferView->u.integer;

    out_data->byte_offset = 0;
    if (json_object_get_value(accessor, "byteOffset")) {
        json_value *byteOffset;
        json_get_value(accessor, byteOffset, JSON_VALUE_INTEGER);
        out_data->byte_offset = byteOffset->u.integer;
    }

    out_data->normalized = false;
    if (json_object_get_value(accessor, "normalized")) {
        json_value *normalized;
        json_get_value(accessor, normalized, JSON_VALUE_BOOLEAN);
        out_data->normalized = normalized->u.boolean;
    }

    json_value *count;
    json_get_value(accessor, count, JSON_VALUE_INTEGER);
    out_data->count = count->u.integer;

    json_value *type;
    json_get_value(accessor, type, JSON_VALUE_STRING);
    out_data->type = _accessor_type_from_string(type->u.string.ptr, type->u.string.length);

    u8 type_length = 0;
    switch (out_data->type) {
    case GLTF_ACCESSOR_TYPE_SCALAR:
        type_length = 1;
        break;
    case GLTF_ACCESSOR_TYPE_VEC2:
        type_length = 2;
        break;
    case GLTF_ACCESSOR_TYPE_VEC3:
        type_length = 3;
        break;
    case GLTF_ACCESSOR_TYPE_VEC4:
    case GLTF_ACCESSOR_TYPE_MAT2:
        type_length = 4;
        break;
    case GLTF_ACCESSOR_TYPE_MAT3:
        type_length = 9;
        break;
    case GLTF_ACCESSOR_TYPE_MAT4:
        type_length = 16;
        break;
    }

    if (json_object_get_value(accessor, "max")) {
        json_value *max;
        json_get_value(accessor, max, JSON_VALUE_ARRAY);
        if (max->u.array.length != type_length) {
            fprintf(stderr,
                    "expected array of length %d for max, got %d\n",
                    type_length,
                    max->u.array.length);
            return;
        }
        for (u8 i = 0; i < type_length; i++) {
            if (max->u.array.values[i]->type != JSON_VALUE_NUMBER) {
                fprintf(stderr, "max contains value that is not a number\n");
                return;
            }
            out_data->max.raw[i] = (f32)max->u.array.values[i]->u.number;
        }
    }

    if (json_object_get_value(accessor, "min")) {
        json_value *min;
        json_get_value(accessor, min, JSON_VALUE_ARRAY);
        if (min->u.array.length != type_length) {
            fprintf(stderr,
                    "expected array of length %d for min, got %d\n",
                    type_length,
                    min->u.array.length);
            return;
        }
        for (u8 i = 0; i < type_length; i++) {
            if (min->u.array.values[i]->type != JSON_VALUE_NUMBER) {
                fprintf(stderr, "min contains value that is not a number\n");
                return;
            }
            out_data->min.raw[i] = (f32)min->u.array.values[i]->u.number;
        }
    }
}

static void parse_buffer_view(json_value *buffer_view, gltf_buffer_view *out_data) {
    json_value *buffer;
    json_get_value(buffer_view, buffer, JSON_VALUE_INTEGER);
    out_data->buffer = buffer->u.integer;

    out_data->byte_offset = 0;
    if (json_object_get_value(buffer_view, "byteOffset")) {
        json_value *byteOffset;
        json_get_value(buffer_view, byteOffset, JSON_VALUE_INTEGER);
        out_data->byte_offset = byteOffset->u.integer;
    }

    json_value *byteLength;
    json_get_value(buffer_view, byteLength, JSON_VALUE_INTEGER);
    out_data->byte_length = byteLength->u.integer;

    out_data->byte_stride = -1;
    if (json_object_get_value(buffer_view, "byteStride")) {
        json_value *byteStride;
        json_get_value(buffer_view, byteStride, JSON_VALUE_INTEGER);
        out_data->byte_stride = byteStride->u.integer;
    }

    out_data->target = GLTF_BUFFER_VIEW_TARGET_UNDEFINED;
    if (json_object_get_value(buffer_view, "target")) {
        json_value *target;
        json_get_value(buffer_view, target, JSON_VALUE_INTEGER);
        out_data->target = target->u.integer;
    }
}

static void parse_buffer(json_value *buffer, gltf_buffer *out_data) {
    json_value *byteLength;
    json_get_value(buffer, byteLength, JSON_VALUE_INTEGER);
    out_data->byte_length = byteLength->u.integer;
}

static void parse_gltf(json_value *gltf, gltf_root *out_data) {
    json_value *asset;
    json_get_value(gltf, asset, JSON_VALUE_OBJECT);
    parse_asset(asset, &out_data->asset);

    if (out_data->asset.version_major != 2) {
        fprintf(stderr,
                "Unsupported gltf version: %u.%u\n",
                out_data->asset.version_major,
                out_data->asset.version_minor);
        return;
    }

    json_value *scene;
    json_get_value(gltf, scene, JSON_VALUE_INTEGER);
    u64 default_scene_index = scene->u.integer;

    json_value *scenes;
    json_get_value(gltf, scenes, JSON_VALUE_ARRAY);

    parse_scene(scenes->u.array.values[default_scene_index], &out_data->default_scene);

    json_value *nodes;
    json_get_value(gltf, nodes, JSON_VALUE_ARRAY);

    out_data->node_count = nodes->u.array.length;
    out_data->nodes = malloc(sizeof(*out_data->nodes) * nodes->u.array.length);
    for (u32 i = 0; i < nodes->u.array.length; i++) {
        if (nodes->u.array.values[i]->type != JSON_VALUE_OBJECT) {
            fprintf(stderr, "value at index %d in nodes array is not an object", i);
            return;
        }
        parse_node(nodes->u.array.values[i], &out_data->nodes[i]);
    }

    json_value *meshes;
    json_get_value(gltf, meshes, JSON_VALUE_ARRAY);

    out_data->mesh_count = meshes->u.array.length;
    out_data->meshes = malloc(sizeof(*out_data->meshes) * meshes->u.array.length);
    for (u32 i = 0; i < meshes->u.array.length; i++) {
        if (meshes->u.array.values[i]->type != JSON_VALUE_OBJECT) {
            fprintf(stderr, "value at index %d in meshes array is not an object", i);
            return;
        }
        parse_mesh(meshes->u.array.values[i], &out_data->meshes[i]);
    }

    json_value *accessors;
    json_get_value(gltf, accessors, JSON_VALUE_ARRAY);

    out_data->accessor_count = accessors->u.array.length;
    out_data->accessors = malloc(sizeof(*out_data->accessors) * accessors->u.array.length);
    for (u32 i = 0; i < accessors->u.array.length; i++) {
        if (accessors->u.array.values[i]->type != JSON_VALUE_OBJECT) {
            fprintf(stderr, "value at index %d in accessors array is not an object", i);
            return;
        }
        parse_accessor(accessors->u.array.values[i], &out_data->accessors[i]);
    }

    json_value *bufferViews;
    json_get_value(gltf, bufferViews, JSON_VALUE_ARRAY);

    out_data->buffer_view_count = bufferViews->u.array.length;
    out_data->buffer_views = malloc(sizeof(*out_data->buffer_views) * bufferViews->u.array.length);
    for (u32 i = 0; i < bufferViews->u.array.length; i++) {
        if (bufferViews->u.array.values[i]->type != JSON_VALUE_OBJECT) {
            fprintf(stderr, "value at index %d in bufferViews array is not an object", i);
            return;
        }
        parse_buffer_view(bufferViews->u.array.values[i], &out_data->buffer_views[i]);
    }

    json_value *buffers;
    json_get_value(gltf, buffers, JSON_VALUE_ARRAY);

    out_data->buffer_count = buffers->u.array.length;
    out_data->buffers = malloc(sizeof(*out_data->buffers) * buffers->u.array.length);
    for (u32 i = 0; i < buffers->u.array.length; i++) {
        if (buffers->u.array.values[i]->type != JSON_VALUE_OBJECT) {
            fprintf(stderr, "value at index %d in buffers array is not an object", i);
            return;
        }
        parse_buffer(buffers->u.array.values[i], &out_data->buffers[i]);
    }
}

#undef json_get_value
