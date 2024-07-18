#include "window.h"
#include "renderer/context.h"

#include "GLFW/glfw3.h"

#include <assert.h>
#include <string.h>

void window_init(engine *e, struct window_create_info create_info) {
    window *window = malloc(sizeof(*window));
    memset(window, 0, sizeof(*window));

    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "Failed to initialize GLFW!\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window->handle = glfwCreateWindow(create_info.width, create_info.height, "game", NULL, NULL);

    glfwSetInputMode(window->handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(window->handle, window);

    // initialize renderer
    // we assume every window needs a renderer
    e->window = window;
    e->render_context = context_new(window->handle);
}

void window_cleanup(engine *e) {
    context_cleanup(&e->render_context);
    glfwDestroyWindow(e->window->handle);
    glfwTerminate();
    free(e->window);
}

b8 window_should_close(window *w) { return glfwWindowShouldClose(w->handle); }

void window_update(window *w) {
    glfwSwapBuffers(w->handle);
    glfwPollEvents();
}

static void mouse_pos_callback(GLFWwindow *handle, double x, double y) {
    window *window = glfwGetWindowUserPointer(handle);
    if (!window->mouse_pos_callback) {
        glfwSetCursorPosCallback(handle, NULL);
        return;
    }

    window->mouse_pos_callback(x, y);
}

void window_set_mouse_pos_callback(engine *e, mouse_pos_callback_t *callback) {
    assert(e != NULL);
    assert(e->window != NULL);

    e->window->mouse_pos_callback = callback;
    glfwSetCursorPosCallback(e->window->handle, mouse_pos_callback);
}
