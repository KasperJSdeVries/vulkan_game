#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H

#include "application.h"

VkCommandBuffer begin_single_time_commands(const Application *app);
void end_single_time_commands(const Application *app, VkCommandBuffer command_buffer);

#endif // COMMAND_BUFFER_H
