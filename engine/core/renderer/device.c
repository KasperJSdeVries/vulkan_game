#include "device.h"

#include "containers/darray.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *device_extenstions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#define device_extenstion_count sizeof(device_extenstions) / sizeof(const char *)

#ifdef NDEBUG
#define enable_validation_layers false
#else
#define enable_validation_layers true
#endif

typedef struct {
    b8 graphics;
    b8 present;
    b8 compute;
    b8 transfer;
    const char **device_extension_names; // darray
    b8 sampler_anisotropy;
    b8 discrete_gpu;
} physical_device_requirements;

typedef struct {
    i32 graphics_family_index;
    i32 present_family_index;
    i32 compute_family_index;
    i32 transfer_family_index;
} queue_family_info;

static b8 pick_physical_device(context *context);
static b8 physical_device_meets_requirements(VkPhysicalDevice device,
                                             VkSurfaceKHR surface,
                                             const VkPhysicalDeviceProperties *properties,
                                             const VkPhysicalDeviceFeatures *features,
                                             const physical_device_requirements *requirements,
                                             queue_family_info *queue_family_info,
                                             swapchain_support_info *swapchain_support);

void device_new(context *context) {
    if (!pick_physical_device(context)) {
        fprintf(stderr, "Failed to find physical device");
        exit(EXIT_FAILURE);
    }

    printf("Creating logical device...\n");

    // NOTE: Do not create additional queues for shared indices.
    b8 present_shares_graphics_queue =
        context->device.graphics_queue_index == context->device.present_queue_index;
    b8 transfer_shares_graphics_queue =
        context->device.graphics_queue_index == context->device.transfer_queue_index;
    b8 present_must_share_graphics = false;
    u32 index_count = 1;
    if (!present_shares_graphics_queue) {
        index_count++;
    }
    if (!transfer_shares_graphics_queue) {
        index_count++;
    }
    i32 indices[index_count];
    u8 index = 0;
    indices[index++] = context->device.graphics_queue_index;
    if (!present_shares_graphics_queue) {
        indices[index++] = context->device.present_queue_index;
    }
    if (!transfer_shares_graphics_queue) {
        indices[index++] = context->device.transfer_queue_index;
    }

    VkDeviceQueueCreateInfo queue_create_infos[index_count];
    f32 queue_priorities[] = {1.0, 0.9};

    u32 property_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context->device.physical_device,
                                             &property_count,
                                             NULL);
    VkQueueFamilyProperties properties[property_count];
    vkGetPhysicalDeviceQueueFamilyProperties(context->device.physical_device,
                                             &property_count,
                                             properties);

    for (u32 i = 0; i < index_count; i++) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = indices[i];
        queue_create_infos[i].queueCount = 1;

        if (present_shares_graphics_queue && indices[i] == context->device.present_queue_index) {
            if (properties[context->device.present_queue_index].queueCount > 1) {
                queue_create_infos[i].queueCount = 2;
            } else {
                present_must_share_graphics = true;
            }
        }

        queue_create_infos[i].flags = 0;
        queue_create_infos[i].pNext = 0;
        queue_create_infos[i].pQueuePriorities = queue_priorities;
    }

    const char *extension_names[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
#define extension_name_count sizeof(extension_names) / sizeof(const char *)

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(context->device.physical_device, &features);

    VkPhysicalDeviceFeatures device_features = {
        .samplerAnisotropy = features.samplerAnisotropy,
        .fillModeNonSolid = features.fillModeNonSolid,
    };

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = index_count,
        .pQueueCreateInfos = queue_create_infos,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = extension_name_count,
        .ppEnabledExtensionNames = extension_names,
        .enabledLayerCount = 0,      // deprecated & ignored
        .ppEnabledLayerNames = NULL, // deprecated & ignored
        .pNext = NULL,
    };
#undef extension_name_count

    VK_CHECK(vkCreateDevice(context->device.physical_device,
                            &device_create_info,
                            NULL,
                            &context->device.logical_device));

    printf("Logical device created.\n");

    vkGetDeviceQueue(context->device.logical_device,
                     context->device.graphics_queue_index,
                     0,
                     &context->device.graphics_queue);

    vkGetDeviceQueue(context->device.logical_device,
                     context->device.present_queue_index,
                     present_must_share_graphics ? 0
                     : (context->device.graphics_queue_index == context->device.present_queue_index)
                         ? 1
                         : 0,
                     &context->device.present_queue);

    vkGetDeviceQueue(context->device.logical_device,
                     context->device.transfer_queue_index,
                     0,
                     &context->device.transfer_queue);
    printf("Queues obtained.\n");

    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = context->device.graphics_queue_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    VK_CHECK(vkCreateCommandPool(context->device.logical_device,
                                 &pool_create_info,
                                 NULL,
                                 &context->device.graphics_command_pool));

    printf("Graphics command pool created.\n");
}

void device_destroy(device *device) {
    vkDestroyCommandPool(device->logical_device, device->graphics_command_pool, NULL);
    vkDestroyDevice(device->logical_device, NULL);
}

void device_query_swapchain_support(VkPhysicalDevice physical_device,
                                    VkSurfaceKHR surface,
                                    swapchain_support_info *support_info) {
    // Surface capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device,
                                                       surface,
                                                       &support_info->capabilities));

    // Surface formats
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,
                                                  surface,
                                                  &support_info->format_count,
                                                  0));

    if (support_info->format_count != 0) {
        if (!support_info->formats) {
            support_info->formats = calloc(support_info->format_count, sizeof(VkSurfaceFormatKHR));
        }
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,
                                                      surface,
                                                      &support_info->format_count,
                                                      support_info->formats));
    }

    // Present modes
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,
                                                       surface,
                                                       &support_info->present_mode_count,
                                                       0));
    if (support_info->present_mode_count != 0) {
        if (!support_info->present_modes) {
            support_info->present_modes =
                calloc(support_info->present_mode_count, sizeof(VkPresentModeKHR));
        }
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,
                                                           surface,
                                                           &support_info->present_mode_count,
                                                           support_info->present_modes));
    }
}

b8 device_detect_depth_format(device *device) {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    u32 flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    for (u32 i = 0; i < sizeof(candidates) / sizeof(VkFormat); i++) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device->physical_device, candidates[i], &properties);

        if ((properties.linearTilingFeatures & flags) == flags) {
            device->depth_format = candidates[i];
            return true;
        } else if ((properties.optimalTilingFeatures & flags) == flags) {
            device->depth_format = candidates[i];
            return true;
        }
    }

    return false;
}

static b8 pick_physical_device(context *context) {
    u32 physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, NULL));
    if (physical_device_count == 0) {
        fprintf(stderr, "No devices that support vulkan were found\n");
        return false;
    }

    physical_device_requirements requirements = {
        .graphics = true,
        .present = true,
        .transfer = true,
        .compute = true,
        .sampler_anisotropy = true,
        .discrete_gpu = false,
        .device_extension_names = darray_create(const char *),
    };

    for (u32 i = 0; i < device_extenstion_count; i++) {
        darray_push(requirements.device_extension_names, device_extenstions[i]);
    }

    VkPhysicalDevice physical_devices[physical_device_count];

    VK_CHECK(
        vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices));

    for (u32 i = 0; i < physical_device_count; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_devices[i], &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physical_devices[i], &features);

        VkPhysicalDeviceMemoryProperties memory;
        vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &memory);

        printf("Evaluating device: '%s', index '%u'.\n", properties.deviceName, i);

        queue_family_info queue_info = {0};

        b8 result = physical_device_meets_requirements(physical_devices[i],
                                                       context->surface,
                                                       &properties,
                                                       &features,
                                                       &requirements,
                                                       &queue_info,
                                                       &context->device.swapchain_support);

        if (result) {
            printf("Selected device: '%s'.\n", properties.deviceName);
            // GPU type, etc.
            switch (properties.deviceType) {
            default:
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                printf("GPU type is Unknown.\n");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                printf("GPU type is Integrated.\n");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                printf("GPU type is Discrete.\n");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                printf("GPU type is Virtual.\n");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                printf("GPU type is CPU.\n");
                break;
            }
            // Memory information
            for (u32 j = 0; j < memory.memoryHeapCount; ++j) {
                f32 memory_size_gib =
                    (((f32)memory.memoryHeaps[j].size) / 1024.0f / 1024.0f / 1024.0f);
                if (memory.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    printf("Local GPU memory: %.2f GiB\n", memory_size_gib);
                } else {
                    printf("Shared System memory: %.2f GiB\n", memory_size_gib);
                }
            }

            context->device.physical_device = physical_devices[i];

            context->device.graphics_queue_index = queue_info.graphics_family_index;
            context->device.present_queue_index = queue_info.present_family_index;
            context->device.transfer_queue_index = queue_info.transfer_family_index;
            context->device.compute_queue_index = queue_info.compute_family_index;

            break;
        }
    }

    darray_destroy(requirements.device_extension_names);

    if (!context->device.physical_device) {
        fprintf(stderr, "No physical devices were found which meet the requirements.\n");
        return false;
    }

    printf("Physical device selected.\n");
    return true;
}

static b8 physical_device_meets_requirements(VkPhysicalDevice device,
                                             VkSurfaceKHR surface,
                                             const VkPhysicalDeviceProperties *properties,
                                             const VkPhysicalDeviceFeatures *features,
                                             const physical_device_requirements *requirements,
                                             queue_family_info *queue_family_info,
                                             swapchain_support_info *swapchain_support) {
    queue_family_info->graphics_family_index = -1;
    queue_family_info->present_family_index = -1;
    queue_family_info->transfer_family_index = -1;
    queue_family_info->compute_family_index = -1;

    if (requirements->discrete_gpu) {
        if (properties->deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            printf("Device is not a discrete GPU, and one is required. Skipping.");
            return false;
        }
    }

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    printf("Graphics | Present | Compute | Transfer | Name\n");
    u8 min_transfer_score = 255;
    for (u32 i = 0; i < queue_family_count; i++) {
        u8 current_transfer_score = 0;

        if (queue_family_info->graphics_family_index == -1 &&
            queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queue_family_info->graphics_family_index = i;
            current_transfer_score++;

            VkBool32 supports_present = VK_FALSE;
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present));
            if (supports_present) {
                queue_family_info->present_family_index = i;
                current_transfer_score++;
            }
        }

        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            queue_family_info->compute_family_index = i;
            current_transfer_score++;
        }

        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (current_transfer_score <= min_transfer_score) {
                min_transfer_score = current_transfer_score;
                queue_family_info->transfer_family_index = i;
            }
        }
    }

    if (queue_family_info->present_family_index == -1) {
        for (u32 i = 0; i < queue_family_count; i++) {
            VkBool32 supports_present = VK_FALSE;
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present));
            if (supports_present) {
                queue_family_info->present_family_index = i;

                if (queue_family_info->present_family_index !=
                    queue_family_info->graphics_family_index) {
                    printf("Warning: Different queue index used for present vs graphics queue: "
                           "%u.\n",
                           i);
                }
                break;
            }
        }
    }

    printf(" %7d |  %6d | %7d | %8d | %s\n",
           queue_family_info->graphics_family_index,
           queue_family_info->present_family_index,
           queue_family_info->compute_family_index,
           queue_family_info->transfer_family_index,
           properties->deviceName);

    if ((!requirements->graphics ||
         (requirements->graphics && queue_family_info->graphics_family_index != -1)) &&
        (!requirements->present ||
         (requirements->present && queue_family_info->present_family_index != -1)) &&
        (!requirements->compute ||
         (requirements->compute && queue_family_info->compute_family_index != -1)) &&
        (!requirements->transfer ||
         (requirements->transfer && queue_family_info->transfer_family_index != -1))) {
        printf("Device meets queue requirements.\n");

        device_query_swapchain_support(device, surface, swapchain_support);

        if (swapchain_support->format_count < 1 || swapchain_support->present_mode_count < 1) {
            if (swapchain_support->formats) {
                free(swapchain_support->formats);
            }
            if (swapchain_support->present_modes) {
                free(swapchain_support->present_modes);
            }
            printf("Required swapchain support not present, skipping device.\n");
            return false;
        }

        if (requirements->device_extension_names) {
            u32 available_extension_count = 0;
            VK_CHECK(
                vkEnumerateDeviceExtensionProperties(device, 0, &available_extension_count, NULL));
            VkExtensionProperties available_extensions[available_extension_count];
            if (available_extension_count != 0) {
                VK_CHECK(vkEnumerateDeviceExtensionProperties(device,
                                                              0,
                                                              &available_extension_count,
                                                              available_extensions));

                u32 required_extension_count = darray_length(requirements->device_extension_names);
                for (u32 i = 0; i < required_extension_count; i++) {
                    b8 found = false;
                    for (u32 j = 0; j < available_extension_count; j++) {
                        if (strcmp(requirements->device_extension_names[i],
                                   available_extensions[j].extensionName) == 0) {
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        printf("Required extension not found: '%s', skipping device.\n",
                               requirements->device_extension_names[i]);
                        return false;
                    }
                }
            }
        }

        if (requirements->sampler_anisotropy && !features->samplerAnisotropy) {
            printf("Device does not support samplerAnisotropy, skipping.\n");
            return false;
        }

        return true;
    }

    return false;
}
