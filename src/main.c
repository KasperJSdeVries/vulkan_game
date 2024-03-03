#define GLFW_INCLUDE_VULKAN
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

#define CLAMP(value, min, max) (value < min ? min : (value > max ? max : value))

const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
#define validation_layer_count sizeof(validation_layers) / sizeof(const char *)

const char *device_extenstions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#define device_extenstion_count                                                \
    sizeof(device_extenstions) / sizeof(const char *)

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
    uint32_t graphics_family;
    bool has_graphics_family;
    uint32_t present_family;
    bool has_present_family;
} QueueFamilyIndices;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    uint32_t format_count;
    VkPresentModeKHR *present_modes;
    uint32_t present_mode_count;
} SwapchainSupportDetails;

typedef struct {
    GLFWwindow *window;
    VkSurfaceKHR surface;

    VkInstance instance;

    VkDebugUtilsMessengerEXT debug_messenger;

    VkPhysicalDevice physical_device;
    VkDevice device;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;
    VkImage *swapchain_images;
    uint32_t swapchain_image_count;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
} Application;

static void init_window(Application *app);
static void init_vulkan(Application *app);
static void main_loop(Application *app);
static void cleanup(Application *app);
static void create_instance(Application *app);
static void setup_debug_messenger(Application *app);
static void create_surface(Application *app);
static void pick_physical_device(Application *app);
static void create_logical_device(Application *app);
static void create_swapchain(Application *app);
static int rate_device_suitability(Application *app, VkPhysicalDevice device);
static QueueFamilyIndices findQueueFamilies(Application *app,
                                            VkPhysicalDevice device);
static bool queue_family_indices_is_complete(QueueFamilyIndices indices);
static uint32_t *queue_family_indices_get_unique(QueueFamilyIndices indices);
static SwapchainSupportDetails query_swapchain_support(Application *app,
                                                       VkPhysicalDevice device);
static VkSurfaceFormatKHR
choose_swap_surface_format(const VkSurfaceFormatKHR *available_formats,
                           uint32_t available_format_count);
static VkPresentModeKHR
choose_swap_present_mode(const VkPresentModeKHR *available_present_modes,
                         uint32_t available_present_mode_count);
static VkExtent2D choose_swap_extent(Application *app,
                                     VkSurfaceCapabilitiesKHR capabilities);
static bool check_validation_layer_support();
static bool check_device_extension_support(VkPhysicalDevice device);
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
    create_surface(app);
    pick_physical_device(app);
    create_logical_device(app);
    create_swapchain(app);
}

static void main_loop(Application *app) {
    while (!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
    }
}

static void cleanup(Application *app) {
    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);

    vkDestroyDevice(app->device, NULL);

    vkDestroySurfaceKHR(app->instance, app->surface, NULL);

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

static void create_surface(Application *app) {
    VK_CHECK(glfwCreateWindowSurface(app->instance, app->window, NULL,
                                     &app->surface));
}

static void pick_physical_device(Application *app) {
    app->physical_device = VK_NULL_HANDLE;

    uint32_t device_count;
    vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);

    if (device_count == 0) {
        fprintf(stderr, "failed to find GPU with vulkan support\n");
        exit(EXIT_FAILURE);
    }

    VkPhysicalDevice devices[device_count];
    vkEnumeratePhysicalDevices(app->instance, &device_count, devices);

    int max_rating = 0;
    for (uint32_t i = 0; i < device_count; ++i) {
        int rating;
        if ((rating = rate_device_suitability(app, devices[i])) > max_rating) {
            app->physical_device = devices[i];
            max_rating = rating;
            break;
        }
    }

    if (app->physical_device == VK_NULL_HANDLE) {
        fprintf(stderr, "failed to find suitable GPU\n");
        exit(EXIT_FAILURE);
    }
}

static void create_logical_device(Application *app) {
    QueueFamilyIndices indices = findQueueFamilies(app, app->physical_device);

    uint32_t *unique_indices = queue_family_indices_get_unique(indices);

    VkDeviceQueueCreateInfo queue_create_infos[darray_length(unique_indices)];

    float queue_priority = 1.0f;
    for (int i = 0; i < darray_length(unique_indices); i++) {
        queue_create_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_indices[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    VkPhysicalDeviceFeatures device_features = {0};

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = darray_length(unique_indices),
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = device_extenstion_count,
        .ppEnabledExtensionNames = device_extenstions,
    };

    if (enable_validation_layers) {
        create_info.enabledLayerCount = validation_layer_count;
        create_info.ppEnabledLayerNames = validation_layers;
    } else {
        create_info.enabledLayerCount = 0;
    }

    VK_CHECK(
        vkCreateDevice(app->physical_device, &create_info, NULL, &app->device));

    vkGetDeviceQueue(app->device, indices.graphics_family, 0,
                     &app->graphics_queue);
    vkGetDeviceQueue(app->device, indices.present_family, 0,
                     &app->present_queue);

    darray_destroy(unique_indices);
}

static void create_swapchain(Application *app) {
    SwapchainSupportDetails swapchain_support =
        query_swapchain_support(app, app->physical_device);

    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(
        swapchain_support.formats, swapchain_support.format_count);
    VkPresentModeKHR present_mode = choose_swap_present_mode(
        swapchain_support.present_modes, swapchain_support.present_mode_count);
    VkExtent2D extent = choose_swap_extent(app, swapchain_support.capabilities);
    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = swapchain_support.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    QueueFamilyIndices indices = findQueueFamilies(app, app->physical_device);
    uint32_t queue_family_indices[] = {indices.graphics_family,
                                       indices.present_family};

    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }

    VK_CHECK(
        vkCreateSwapchainKHR(app->device, &create_info, NULL, &app->swapchain));

    vkGetSwapchainImagesKHR(app->device, app->swapchain,
                            &app->swapchain_image_count, NULL);
    app->swapchain_images = calloc(app->swapchain_image_count, sizeof(VkImage));
    vkGetSwapchainImagesKHR(app->device, app->swapchain,
                            &app->swapchain_image_count, app->swapchain_images);

    app->swapchain_image_format = surface_format.format;
    app->swapchain_extent = extent;
}

static int rate_device_suitability(Application *app, VkPhysicalDevice device) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    QueueFamilyIndices indices = findQueueFamilies(app, device);

    if (!queue_family_indices_is_complete(indices)) {
        return -1;
    }

    if (!check_device_extension_support(device)) {
        return -1;
    }

    SwapchainSupportDetails swapchain_support =
        query_swapchain_support(app, device);
    if (!swapchain_support.format_count ||
        !swapchain_support.present_mode_count) {
        return -1;
    }

    if (!device_features.geometryShader) {
        return -1;
    }

    int score = 0;

    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 10000;
    }

    // Maximum possible size of textures.
    score += device_properties.limits.maxImageDimension2D;

    printf("%-40s | %d\n", device_properties.deviceName, score);
    return score;
}

static QueueFamilyIndices findQueueFamilies(Application *app,
                                            VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             queue_families);

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.has_graphics_family = true;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, app->surface,
                                             &present_support);
        if (present_support) {
            indices.present_family = i;
            indices.has_present_family = true;
        }

        if (queue_family_indices_is_complete(indices)) {
            break;
        }
    }

    return indices;
}

static bool queue_family_indices_is_complete(QueueFamilyIndices indices) {
    return indices.has_graphics_family && indices.has_present_family;
}

static uint32_t *queue_family_indices_get_unique(QueueFamilyIndices indices) {
    uint32_t index_numbers[] = {indices.graphics_family,
                                indices.present_family};
    size_t index_number_count = sizeof(index_numbers) / sizeof(uint32_t);
    uint32_t *unique_indices = darray_create(uint32_t);

    for (int i = 0; i < index_number_count; i++) {
        bool is_unique = true;
        for (int j = 0; j < darray_length(unique_indices); j++) {
            if (index_numbers[i] == unique_indices[j]) {
                is_unique = false;
                break;
            }
        }
        if (is_unique) {
            darray_push(unique_indices, index_numbers[i]);
        }
    }

    return unique_indices;
}

static SwapchainSupportDetails
query_swapchain_support(Application *app, VkPhysicalDevice device) {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, app->surface,
                                              &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->surface,
                                         &details.format_count, NULL);

    if (details.format_count != 0) {
        details.formats =
            calloc(details.format_count, sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, app->surface, &details.format_count, details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, app->surface, &details.present_mode_count, NULL);

    if (details.present_mode_count != 0) {
        details.present_modes =
            calloc(details.format_count, sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->surface,
                                                  &details.present_mode_count,
                                                  details.present_modes);
    }

    return details;
}

static VkSurfaceFormatKHR
choose_swap_surface_format(const VkSurfaceFormatKHR *available_formats,
                           uint32_t available_format_count) {
    for (int i = 0; i < available_format_count; i++) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_formats[i].colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_formats[i];
        }
    }

    return available_formats[0];
}

static VkPresentModeKHR
choose_swap_present_mode(const VkPresentModeKHR *available_present_modes,
                         uint32_t available_present_mode_count) {
    for (int i = 0; i < available_present_mode_count; i++) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_modes[i];
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_swap_extent(Application *app,
                                     VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(app->window, &width, &height);
    VkExtent2D actual_extent = {
        .width = CLAMP(width, capabilities.minImageExtent.width,
                       capabilities.maxImageExtent.width),
        .height = CLAMP(height, capabilities.minImageExtent.height,
                        capabilities.maxImageExtent.height),
    };

    return actual_extent;
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

static bool check_device_extension_support(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    VkExtensionProperties available_extensions[extension_count];
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count,
                                         available_extensions);

    for (int i = 0; i < device_extenstion_count; i++) {
        bool extension_found = false;

        for (int j = 0; j < extension_count; j++) {
            if (strcmp(device_extenstions[i],
                       available_extensions[j].extensionName) == 0) {
                extension_found = true;
                break;
            }
        }

        if (!extension_found) {
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
