#include "swapchain.h"

#include "command_buffer.h"
#include "defines.h"
#include "device.h"

#include "vulkan/vulkan_core.h"

#include <stdint.h>
#include <stdlib.h>

static void create(context *context, u32 width, u32 height, swapchain *swapchain);
static void destroy(context *context, swapchain *swapchain);

void swapchain_create(context *context, u32 width, u32 height, swapchain *swapchain) {
    create(context, width, height, swapchain);
}

void swapchain_recreate(context *context, u32 width, u32 height, swapchain *swapchain) {
    destroy(context, swapchain);
    create(context, width, height, swapchain);
}

void swapchain_destroy(context *context, swapchain *swapchain) { destroy(context, swapchain); }

static void create(context *context, u32 width, u32 height, swapchain *swapchain) {
    VkExtent2D swapchain_extent = {width, height};

    b8 found = false;
    for (u32 i = 0; i < context->device.swapchain_support.format_count; i++) {
        VkSurfaceFormatKHR format = context->device.swapchain_support.formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            swapchain->image_format = format;
            found = true;
            break;
        }
    }

    if (!found) {
        swapchain->image_format = context->device.swapchain_support.formats[0];
    }

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    // TODO: Add toggle for vsync
    if (false /* check for vsync toggle */) {
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
        // prefer mailbox over fifo
        for (u32 i = 0; i < context->device.swapchain_support.present_mode_count; i++) {
            VkPresentModeKHR mode = context->device.swapchain_support.present_modes[i];
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = mode;
            }
        }
    }

    // requery swapchain support
    device_query_swapchain_support(context->device.physical_device,
                                   context->surface,
                                   &context->device.swapchain_support);

    if (context->device.swapchain_support.capabilities.currentExtent.width != UINT32_MAX) {
        swapchain_extent = context->device.swapchain_support.capabilities.currentExtent;
    }

    VkExtent2D min = context->device.swapchain_support.capabilities.minImageExtent;
    VkExtent2D max = context->device.swapchain_support.capabilities.maxImageExtent;
    swapchain_extent.width = CLAMP(swapchain_extent.width, min.width, max.width);
    swapchain_extent.height = CLAMP(swapchain_extent.height, min.height, max.height);

    u32 image_count = context->device.swapchain_support.capabilities.minImageCount + 1;
    if (context->device.swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > context->device.swapchain_support.capabilities.maxImageCount) {
        image_count = context->device.swapchain_support.capabilities.maxImageCount;
    }

    swapchain->max_frames_in_flight = image_count - 1;

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->surface,
        .minImageCount = image_count,
        .imageFormat = swapchain->image_format.format,
        .imageColorSpace = swapchain->image_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = context->device.swapchain_support.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    if (context->device.graphics_queue_index != context->device.present_queue_index) {
        u32 queue_family_indices[] = {
            context->device.graphics_queue_index,
            context->device.present_queue_index,
        };
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    }

    VK_CHECK(vkCreateSwapchainKHR(context->device.logical_device,
                                  &swapchain_create_info,
                                  NULL,
                                  &swapchain->handle));

    context->current_frame = 0;

    swapchain->image_count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(context->device.logical_device,
                                     swapchain->handle,
                                     &swapchain->image_count,
                                     NULL));
    swapchain->images = calloc(swapchain->image_count, sizeof(VkImage));
    VK_CHECK(vkGetSwapchainImagesKHR(context->device.logical_device,
                                     swapchain->handle,
                                     &swapchain->image_count,
                                     swapchain->images));

    if (!swapchain->image_views) {
        swapchain->image_views = calloc(swapchain->image_count, sizeof(VkImageView));

        for (u32 i = 0; i < swapchain->image_count; i++) {
            VkImageViewCreateInfo view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchain->images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapchain->image_format.format,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            };

            VK_CHECK(vkCreateImageView(context->device.logical_device,
                                       &view_info,
                                       NULL,
                                       &swapchain->image_views[i]));
        }
    }

    if (!swapchain->depth_image) {
        VkImageCreateInfo depth_image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent =
                {
                    .width = swapchain_extent.width,
                    .height = swapchain_extent.height,
                    .depth = 1,
                },
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = context->device.depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .samples = VK_SAMPLE_COUNT_1_BIT,
        };

        VK_CHECK(vkCreateImage(context->device.logical_device,
                               &depth_image_info,
                               NULL,
                               &swapchain->depth_image));

        VkMemoryRequirements depth_memory_requirements = {};
        vkGetImageMemoryRequirements(context->device.logical_device,
                                     swapchain->depth_image,
                                     &depth_memory_requirements);
        i32 memory_type = context->find_memory_index(context,
                                                     depth_memory_requirements.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == -1) {
            fprintf(stderr, "Required memory type not found. Image not valid.\n");
        }

        VkMemoryAllocateInfo depth_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = depth_memory_requirements.size,
            .memoryTypeIndex = memory_type,
        };

        VK_CHECK(vkAllocateMemory(context->device.logical_device,
                                  &depth_alloc_info,
                                  NULL,
                                  &swapchain->depth_image_memory))

        VK_CHECK(vkBindImageMemory(context->device.logical_device,
                                   swapchain->depth_image,
                                   swapchain->depth_image_memory,
                                   0));

        VkImageViewCreateInfo depth_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain->depth_image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = context->device.depth_format,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        VK_CHECK(vkCreateImageView(context->device.logical_device,
                                   &depth_view_info,
                                   NULL,
                                   &swapchain->depth_image_view));

        VkCommandBuffer command_buffer = begin_single_time_commands(context);

        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = context->device.graphics_queue_index,
            .dstQueueFamilyIndex = context->device.graphics_queue_index,
            .image = swapchain->depth_image,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
                                  ((context->device.depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                    context->device.depth_format == VK_FORMAT_D24_UNORM_S8_UINT)
                                       ? VK_IMAGE_ASPECT_STENCIL_BIT
                                       : 0),
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        };

        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                             0,
                             0,
                             NULL,
                             0,
                             NULL,
                             1,
                             &barrier);

        end_single_time_commands(context, command_buffer);
    }

    if (!swapchain->framebuffers) {
        swapchain->framebuffers = calloc(swapchain->image_count, sizeof(VkFramebuffer));

        for (u32 i = 0; i < swapchain->image_count; i++) {
            VkImageView attachments[] = {
                swapchain->image_views[i],
                swapchain->depth_image_view,
            };

            VkFramebufferCreateInfo framebuffer_info = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = context->render_pass,
                .attachmentCount = sizeof(attachments) / sizeof(VkImageView),
                .pAttachments = attachments,
                .width = swapchain_extent.width,
                .height = swapchain_extent.height,
                .layers = 1,
            };

            VK_CHECK(vkCreateFramebuffer(context->device.logical_device,
                                         &framebuffer_info,
                                         NULL,
                                         &swapchain->framebuffers[i]));
        }
    }
}

static void destroy(context *context, swapchain *swapchain) {
    vkDeviceWaitIdle(context->device.logical_device);

    vkDestroyImageView(context->device.logical_device, swapchain->depth_image_view, NULL);
    vkDestroyImage(context->device.logical_device, swapchain->depth_image, NULL);
    vkFreeMemory(context->device.logical_device, swapchain->depth_image_memory, NULL);

    swapchain->depth_image_view = VK_NULL_HANDLE;
    swapchain->depth_image = VK_NULL_HANDLE;
    swapchain->depth_image_memory = VK_NULL_HANDLE;

    for (u32 i = 0; i < swapchain->image_count; i++) {
        vkDestroyFramebuffer(context->device.logical_device, swapchain->framebuffers[i], NULL);
    }

    for (u32 i = 0; i < swapchain->image_count; i++) {
        vkDestroyImageView(context->device.logical_device, swapchain->image_views[i], NULL);
    }

    free(swapchain->images);
    swapchain->images = NULL;
    free(swapchain->image_views);
    swapchain->image_views = NULL;
    free(swapchain->framebuffers);
    swapchain->framebuffers = NULL;

    vkDestroySwapchainKHR(context->device.logical_device, swapchain->handle, NULL);
}
