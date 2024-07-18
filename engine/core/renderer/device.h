#ifndef DEVICE_H
#define DEVICE_H

#include "types.h"

void device_new(context *context);
void device_destroy(device *device);

void device_query_swapchain_support(VkPhysicalDevice physical_device,
                                    VkSurfaceKHR surface,
                                    swapchain_support_info *support_info);

b8 device_detect_depth_format(device *device);

#endif // DEVICE_H
