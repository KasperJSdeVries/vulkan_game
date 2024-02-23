#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "darray.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1280
#define HEIGHT 720

#define VK_CHECK(fun)                                                          \
    if (fun != VK_SUCCESS) {                                                   \
        fprintf(stderr, "%s:%d: \"%s\"\n", __FILE_NAME__, __LINE__, #fun);     \
        exit(EXIT_FAILURE);                                                    \
    }

const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
#define validation_layer_count sizeof(validation_layers) / sizeof(const char *)

#ifdef NDEBUG
#define enable_validation_layers false
#else
#define enable_validation_layers true
#endif

#define check_validation()                                                     \
    if (enable_validation_layers && !check_validation_layer_support()) {       \
        fprintf(stderr,                                                        \
                "%s:%d: validation layers requested, but not available\n",     \
                __FILE_NAME__, __LINE__);                                      \
        exit(EXIT_FAILURE);                                                    \
    }

typedef struct {
    GLFWwindow *window;

    VkInstance instance;

    VkDebugUtilsMessengerEXT debug_messenger;
} Application;

static void init_window(Application *app);
static void init_vulkan(Application *app);
static void main_loop(Application *app);
static void cleanup(Application *app);
static void create_instance(Application *app);
static void setup_debug_messenger(Application *app);
static bool check_validation_layer_support();
static void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *create_info);
static const char **get_required_extensions();
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

VkResult create_debug_utils_messenger_ext(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
void destroy_debug_utils_messenger_ext(
    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

int main() {
    Application app = {0};

    init_window(&app);
    init_vulkan(&app);

    main_loop(&app);

    cleanup(&app);
}

static void init_window(Application *app) {
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "Failed to initialize GLFW!\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    app->window = glfwCreateWindow(WIDTH, HEIGHT, "game", NULL, NULL);
}

static void init_vulkan(Application *app) {
    create_instance(app);
    setup_debug_messenger(app);
}

static void main_loop(Application *app) {
    while (!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
    }
}

static void cleanup(Application *app) {
    if (enable_validation_layers) {
        destroy_debug_utils_messenger_ext(app->instance, app->debug_messenger,
                                          NULL);
    }

    vkDestroyInstance(app->instance, NULL);

    glfwDestroyWindow(app->window);

    glfwTerminate();
}

static void create_instance(Application *app) {
    check_validation();

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello, Triangle",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    const char **required_extensions = get_required_extensions();

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = darray_length(required_extensions),
        .ppEnabledExtensionNames = required_extensions,
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
    if (enable_validation_layers) {
        create_info.enabledLayerCount = validation_layer_count;
        create_info.ppEnabledLayerNames = validation_layers;

        populate_debug_messenger_create_info(&debug_create_info);
        create_info.pNext = &debug_create_info;
    } else {
        create_info.enabledLayerCount = 0;

        create_info.pNext = NULL;
    }

    VK_CHECK(vkCreateInstance(&create_info, NULL, &app->instance));
}

static void setup_debug_messenger(Application *app) {
    if (!enable_validation_layers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT create_info;
    populate_debug_messenger_create_info(&create_info);

    VK_CHECK(create_debug_utils_messenger_ext(app->instance, &create_info, NULL,
                                              &app->debug_messenger));
}

static bool check_validation_layer_support() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    VkLayerProperties available_layers[layer_count];
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    for (int i = 0; i < validation_layer_count; i++) {
        bool layer_found = false;

        for (int j = 0; j < layer_count; j++) {
            if (strcmp(validation_layers[i], available_layers[j].layerName) ==
                0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }

    return true;
}

static void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *create_info) {
    *create_info = (VkDebugUtilsMessengerCreateInfoEXT){
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };
}

/**
 * @return darray
 */
static const char **get_required_extensions() {
    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions;

    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    const char **required_extensions = darray_create(const char *);
    for (int i = 0; i < glfw_extension_count; ++i) {
        darray_push(required_extensions, glfw_extensions[i]);
    }

    if (enable_validation_layers) {
        const char *debug_extension_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        darray_push(required_extensions, debug_extension_name);
    }

    return required_extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
    fprintf(stderr, "validation layer: %s\n", callback_data->pMessage);

    return VK_FALSE;
}
