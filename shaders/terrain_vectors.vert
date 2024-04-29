#version 450

layout (location = 0) out vec3 out_colour;

layout (location = 0) in float aAltitude;
layout (location = 1) in vec3 displacement;

layout (binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout (push_constant, std430) uniform Params {
    float vertices_per_run;
    float vertices_per_run_not_degenerate;
} params;

void main() {
    float run_index = mod(gl_VertexIndex, params.vertices_per_run);
    float clamped_index = clamp(run_index - 1.0, 0.0, params.vertices_per_run_not_degenerate);

    float x_pos = floor(clamped_index / 2.0);

    float z_pos = floor(gl_VertexIndex / params.vertices_per_run);

    z_pos += mod(clamped_index, 2.0);

    vec3 pos = vec3(x_pos, aAltitude, z_pos) + displacement;
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(pos, 1.0);

    out_colour = pos;
}
