#include "camera.h"

Camera camera_create(vec3s position) {
    Camera camera = {
        position,
        {{0.0, 0.0, -1.0}},
        {{0.0, 1.0, 0.0}},
    };

    return camera;
}

void camera_process_input(GLFWwindow *window, Camera *camera, f32 delta_time) {
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

// TODO: make this per-camera
f32 pitch = 0, yaw = -90.0f;
f32 last_x = 400, last_y = 300;
void camera_mouse_callback(GLFWwindow *window, double x_position, double y_position) {
    (void)window;

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

UniformBufferObject camera_create_ubo(const context *render_context, Camera camera) {
    vec3s direction = {{
        cos(glm_rad(yaw)) * cos(glm_rad(pitch)),
        sin(glm_rad(pitch)),
        sin(glm_rad(yaw)) * cos(glm_rad(pitch)),
    }};
    camera.front = glms_vec3_normalize(direction);

    UniformBufferObject ubo = {
        .model = glms_mat4_identity(),
        // https://learnopengl.com/Getting-started/Camera
        .view =
            glms_lookat(camera.position, glms_vec3_add(camera.position, camera.front), camera.up),
        .projection = glms_perspective(glm_rad(45.0f),
                                       (float)render_context->framebuffer_width /
                                           (float)render_context->framebuffer_height,
                                       0.1f,
                                       1000.0f),
    };

    ubo.projection.m11 *= -1;

    return ubo;
}
