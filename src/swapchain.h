#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "types.h"

void swapchain_create(context *context, u32 width, u32 height, swapchain *swapchain);
void swapchain_recreate(context *context, u32 width, u32 height, swapchain *swapchain);
void swapchain_destroy(context *context, swapchain *swapchain);

#endif // SWAPCHAIN_H
