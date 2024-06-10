#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    outUV = inUV;
    outColor = vec3(0.0, 0.0, 0.0);
}
