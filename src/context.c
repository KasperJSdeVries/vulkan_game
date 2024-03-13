#include "context.h"

#include "darray.h"
#include "device.h"
#include "swapchain.h"
#include "types.h"
#include "vulkan/vulkan_core.h"
#include <stdint.h>

const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
#define validation_layer_count sizeof(validation_layers) / sizeof(const char *)

static VkResult create_debug_utils_messenger_ext(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger);
static void destroy_debug_utils_messenger_ext(VkInstance instance,
                                              VkDebugUtilsMessengerEXT debugMessenger,
                                              const VkAllocationCallbacks *pAllocator);
static void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT *create_info);
static const char **get_required_extensions();
static i32 find_memory_index(const context *context, u32 type_filter, u32 property_flags);
static void create_render_pass(context *context);

context context_new(GLFWwindow *window) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    context context = {
        .find_memory_index = find_memory_index,
        .framebuffer_width = width,
        .framebuffer_height = height,
    };

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Game",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "Vulkan Game",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    const char **required_extensions = get_required_extensions();

#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
    populate_debug_messenger_create_info(&debug_create_info);
#endif

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = darray_length(required_extensions),
        .ppEnabledExtensionNames = required_extensions,
#ifndef NDEBUG
        .enabledLayerCount = validation_layer_count,
        .ppEnabledLayerNames = validation_layers,
        .pNext = &debug_create_info,
#endif
    };

    VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &context.instance));
    darray_destroy(required_extensions);

#ifndef NDEBUG
    VK_CHECK(create_debug_utils_messenger_ext(context.instance,
                                              &debug_create_info,
                                              NULL,
                                              &context.debug_messenger));
#endif

    VK_CHECK(glfwCreateWindowSurface(context.instance, window, NULL, &context.surface));

    device_new(&context);

    if (!device_detect_depth_format(&context.device)) {
        context.device.depth_format = VK_FORMAT_UNDEFINED;
        fprintf(stderr, "Failed to find a supported format!\n");
        exit(EXIT_FAILURE);
    }

    create_render_pass(&context);

    swapchain_create(&context,
                     context.framebuffer_width,
                     context.framebuffer_height,
                     &context.swapchain);

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.device.graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };

    VK_CHECK(vkAllocateCommandBuffers(context.device.logical_device,
                                      &alloc_info,
                                      context.graphics_command_buffers));

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(context.device.logical_device,
                                   &semaphore_info,
                                   NULL,
                                   &context.image_available_semaphores[i]))
        VK_CHECK(vkCreateSemaphore(context.device.logical_device,
                                   &semaphore_info,
                                   NULL,
                                   &context.render_finished_semaphores[i]))
        VK_CHECK(vkCreateFence(context.device.logical_device,
                               &fence_info,
                               NULL,
                               &context.in_flight_fences[i]))
    }

    return context;
}

void context_on_resized(context *context, u32 width, u32 height) {
    context->framebuffer_width = width;
    context->framebuffer_height = height;
    context->framebuffer_size_generation++;
}

void context_begin_main_loop(context *context) {}

VkCommandBuffer context_begin_frame(context *context) {
context_begin_frame_start:

    vkWaitForFences(context->device.logical_device,
                    1,
                    &context->in_flight_fences[context->current_frame],
                    VK_TRUE,
                    UINT64_MAX);

    VkResult result =
        vkAcquireNextImageKHR(context->device.logical_device,
                              context->swapchain.handle,
                              UINT64_MAX,
                              context->image_available_semaphores[context->current_frame],
                              VK_NULL_HANDLE,
                              &context->image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain_recreate(context,
                           context->framebuffer_width,
                           context->framebuffer_height,
                           &context->swapchain);
        goto context_begin_frame_start;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Failed to acquire swapchain image!\n");
        exit(EXIT_FAILURE);
    }

    vkResetFences(context->device.logical_device,
                  1,
                  &context->in_flight_fences[context->current_frame]);

    VkCommandBuffer command_buffer = context->graphics_command_buffers[context->current_frame];
    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    VkClearValue clear_values[] = {
        (VkClearValue){.color = {{0.2f, 0.2f, 0.2f, 1.0f}}},
        (VkClearValue){.depthStencil = {1.0f, 0}},
    };

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = context->render_pass,
        .framebuffer = context->swapchain.framebuffers[context->image_index],
        .renderArea =
            {
                .offset = {0, 0},
                .extent = {context->framebuffer_width, context->framebuffer_height},
            },
        .clearValueCount = sizeof(clear_values) / sizeof(VkClearValue),
        .pClearValues = clear_values,
    };

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)context->framebuffer_width,
        .height = (float)context->framebuffer_height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {context->framebuffer_width, context->framebuffer_height},
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    return context->graphics_command_buffers[context->current_frame];
}

void context_end_frame(context *context) {
    VkCommandBuffer command_buffer = context->graphics_command_buffers[context->current_frame];

    vkCmdEndRenderPass(command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer));

    VkSemaphore wait_semaphores[] = {context->image_available_semaphores[context->current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSemaphore signal_semaphores[] = {context->render_finished_semaphores[context->current_frame]};

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &context->graphics_command_buffers[context->current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores,
    };

    VK_CHECK(vkQueueSubmit(context->device.graphics_queue,
                           1,
                           &submit_info,
                           context->in_flight_fences[context->current_frame]));

    VkSwapchainKHR swapchains[] = {context->swapchain.handle};

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &context->image_index,
        .pResults = NULL,
    };

    VkResult result = vkQueuePresentKHR(context->device.present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        context->framebuffer_resized) {
        context->framebuffer_resized = false;
        swapchain_recreate(context,
                           context->framebuffer_width,
                           context->framebuffer_height,
                           &context->swapchain);
    } else
        VK_CHECK(result);

    context->current_frame = (context->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void context_end_main_loop(context *context) { vkDeviceWaitIdle(context->device.logical_device); }

void context_cleanup(context *context) {
    swapchain_destroy(context, &context->swapchain);

    vkDestroyRenderPass(context->device.logical_device, context->render_pass, NULL);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(context->device.logical_device,
                           context->image_available_semaphores[i],
                           NULL);
        vkDestroySemaphore(context->device.logical_device,
                           context->render_finished_semaphores[i],
                           NULL);
        vkDestroyFence(context->device.logical_device, context->in_flight_fences[i], NULL);
    }

    device_destroy(&context->device);

    vkDestroySurfaceKHR(context->instance, context->surface, NULL);
    context->surface = NULL;

#ifndef NDEBUG
    destroy_debug_utils_messenger_ext(context->instance, context->debug_messenger, NULL);
    context->debug_messenger = NULL;
#endif

    vkDestroyInstance(context->instance, NULL);
    context->instance = NULL;
}

void context_create_buffer(const context *context,
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

    VK_CHECK(vkCreateBuffer(context->device.logical_device, &buffer_info, NULL, buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(context->device.logical_device, *buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex =
            context->find_memory_index(context, memory_requirements.memoryTypeBits, properties),
    };

    VK_CHECK(vkAllocateMemory(context->device.logical_device, &alloc_info, NULL, buffer_memory));

    vkBindBufferMemory(context->device.logical_device, *buffer, *buffer_memory, 0);
}

static VkResult create_debug_utils_messenger_ext(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
                                                                  "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void destroy_debug_utils_messenger_ext(VkInstance instance,
                                              VkDebugUtilsMessengerEXT debugMessenger,
                                              const VkAllocationCallbacks *pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
    fprintf(stderr, "validation layer: %s\n", callback_data->pMessage);

    return VK_FALSE;
}

static void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT *create_info) {
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
 * @returns darray
 */
static const char **get_required_extensions() {
    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions;

    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    const char **required_extensions = darray_create(const char *);
    for (int i = 0; i < glfw_extension_count; ++i) {
        darray_push(required_extensions, glfw_extensions[i]);
    }

#ifndef NDEBUG
    const char *debug_extension_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    darray_push(required_extensions, debug_extension_name);
#endif

    return required_extensions;
}

static i32 find_memory_index(const context *context, u32 type_filter, u32 property_flags) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(context->device.physical_device, &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags) {
            return i;
        }
    }

    printf("WARN: Failed to find suitable memory type\n");
    return -1;
}

static void create_render_pass(context *context) {
    VkSurfaceFormatKHR swapchain_image_format;
    b8 found = false;

    for (int i = 0; i < context->device.swapchain_support.format_count; i++) {
        if (context->device.swapchain_support.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            context->device.swapchain_support.formats[i].colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            swapchain_image_format = context->device.swapchain_support.formats[i];
            found = true;
            break;
        }
    }

    if (!found) {
        swapchain_image_format = context->device.swapchain_support.formats[0];
    }

    VkAttachmentDescription color_attachment = {
        .format = swapchain_image_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_reference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription depth_attachment = {
        .format = context->device.depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_attachment_reference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_reference,
        .pDepthStencilAttachment = &depth_attachment_reference,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = sizeof(attachments) / sizeof(VkAttachmentDescription),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    VK_CHECK(vkCreateRenderPass(context->device.logical_device,
                                &render_pass_info,
                                NULL,
                                &context->render_pass));
}
