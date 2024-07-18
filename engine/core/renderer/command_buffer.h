#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H

#include "types.h"

VkCommandBuffer begin_single_time_commands(const context *context);
void end_single_time_commands(const context *context, VkCommandBuffer command_buffer);

#endif // COMMAND_BUFFER_H
