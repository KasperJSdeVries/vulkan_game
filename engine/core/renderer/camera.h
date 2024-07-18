#ifndef CAMERA_H
#define CAMERA_H

#include "defines.h"
#include "types.h"

typedef struct {
    mat4s model;
    mat4s view;
    mat4s projection;
} UniformBufferObject;

typedef struct {
    vec3s position;
    vec3s front;
    vec3s up;
} Camera;

Camera camera_create(vec3s position);
void camera_mouse_callback(double x_position, double y_position);
void camera_process_input(GLFWwindow *window, Camera *camera, f32 delta_time);
UniformBufferObject camera_create_ubo(const context *render_context, Camera camera);

#endif // CAMERA_H
