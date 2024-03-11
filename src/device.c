#include "device.h"

#include "application.h"
#include "darray.h"
#include "vulkan/vulkan_core.h"
#include <string.h>

static const char *device_extenstions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#define device_extenstion_count sizeof(device_extenstions) / sizeof(const char *)

#ifdef NDEBUG
#define enable_validation_layers false
#else
#define enable_validation_layers true
#endif

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

struct device_t {
    VkDevice handle;
    VkPhysicalDevice physical_device;

    i32 graphics_queue_index;
    i32 present_queue_index;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkCommandPool graphics_command_pool;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;
};

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties);
static void pick_physical_device(Application *app);
static void create_logical_device(Application *app);
static int rate_device_suitability(Application *app, VkPhysicalDevice device);
static bool check_device_extension_support(VkPhysicalDevice device);
static QueueFamilyIndices find_queue_families(Application *app, VkPhysicalDevice device);
static bool queue_family_indices_is_complete(QueueFamilyIndices indices);
static uint32_t *queue_family_indices_get_unique(QueueFamilyIndices indices);
static SwapchainSupportDetails query_swapchain_support(Application *app, VkPhysicalDevice device);
static void swapchain_support_details_destroy(SwapchainSupportDetails swapchain_support);

VkDevice device_get_handle(device *device) { return device->handle; }

void device_create_buffer(device *device,
                          VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VkBuffer *buffer,
                          VkDeviceMemory *buffer_memory) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VK_CHECK(vkCreateBuffer(device->handle, &buffer_info, NULL, buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device->handle, *buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = find_memory_type(device->physical_device,
                                            memory_requirements.memoryTypeBits,
                                            properties),
    };

    VK_CHECK(vkAllocateMemory(device->handle, &alloc_info, NULL, buffer_memory));

    vkBindBufferMemory(device->handle, *buffer, *buffer_memory, 0);
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
    QueueFamilyIndices indices = find_queue_families(app, app->physical_device);

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

    VkPhysicalDeviceFeatures device_features = {
        .samplerAnisotropy = VK_TRUE,
    };

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = darray_length(unique_indices),
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = device_extenstion_count,
        .ppEnabledExtensionNames = device_extenstions,
        .enabledLayerCount = 0,      // deprecated & ignored
        .ppEnabledLayerNames = NULL, // deprecated & ignored
    };

    VK_CHECK(vkCreateDevice(app->physical_device, &create_info, NULL, &app->device));

    vkGetDeviceQueue(app->device, indices.graphics_family, 0, &app->graphics_queue);
    vkGetDeviceQueue(app->device, indices.present_family, 0, &app->present_queue);

    darray_destroy(unique_indices);
}

static int rate_device_suitability(Application *app, VkPhysicalDevice device) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    QueueFamilyIndices indices = find_queue_families(app, device);

    if (!queue_family_indices_is_complete(indices)) {
        return -1;
    }

    if (!check_device_extension_support(device)) {
        return -1;
    }

    SwapchainSupportDetails swapchain_support = query_swapchain_support(app, device);
    if (!swapchain_support.format_count || !swapchain_support.present_mode_count) {
        swapchain_support_details_destroy(swapchain_support);
        return -1;
    }
    swapchain_support_details_destroy(swapchain_support);

    if (!device_features.geometryShader || !device_features.samplerAnisotropy) {
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

static bool check_device_extension_support(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    VkExtensionProperties available_extensions[extension_count];
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    for (int i = 0; i < device_extenstion_count; i++) {
        bool extension_found = false;

        for (int j = 0; j < extension_count; j++) {
            if (strcmp(device_extenstions[i], available_extensions[j].extensionName) == 0) {
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

static QueueFamilyIndices find_queue_families(Application *app, VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.has_graphics_family = true;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, app->surface, &present_support);
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

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)) {
            return i;
        }
    }

    fprintf(stderr, "Failed to find suitable memory type\n");
    exit(EXIT_FAILURE);
}

static bool queue_family_indices_is_complete(QueueFamilyIndices indices) {
    return indices.has_graphics_family && indices.has_present_family;
}

static uint32_t *queue_family_indices_get_unique(QueueFamilyIndices indices) {
    uint32_t index_numbers[] = {indices.graphics_family, indices.present_family};
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

static SwapchainSupportDetails query_swapchain_support(Application *app, VkPhysicalDevice device) {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, app->surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->surface, &details.format_count, NULL);

    if (details.format_count != 0) {
        details.formats = calloc(details.format_count, sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(device,
                                             app->surface,
                                             &details.format_count,
                                             details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                              app->surface,
                                              &details.present_mode_count,
                                              NULL);

    if (details.present_mode_count != 0) {
        details.present_modes = calloc(details.present_mode_count, sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                                  app->surface,
                                                  &details.present_mode_count,
                                                  details.present_modes);
    }

    return details;
}

static void swapchain_support_details_destroy(SwapchainSupportDetails swapchain_support) {
    free(swapchain_support.present_modes);
    free(swapchain_support.formats);
}
