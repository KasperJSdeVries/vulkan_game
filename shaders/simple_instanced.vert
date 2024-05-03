#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in mat4 instance_model;
layout(location = 5) in vec3 instance_colour;

layout(location = 0) out vec3 out_colour;

void main() {
    gl_Position = ubo.proj * ubo.view * instance_model * vec4(inPosition, 1.0);
    out_colour = instance_colour;
}
