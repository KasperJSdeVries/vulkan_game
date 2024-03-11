#ifndef DEVICE_H
#define DEVICE_H

#include <vulkan/vulkan.h>

typedef struct device_t device;

VkDevice device_get_handle(device *device);

void device_create_buffer(device *device,
                          VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VkBuffer *buffer,
                          VkDeviceMemory *buffer_memory);

#endif // DEVICE_H
