#ifndef TASK_H
#define TASK_H

#include "types.h"
#include "user.h"

// Helper functions
const char *command_to_string(CommandType type);
void task_destroy(Task *task);

#endif // TASK_H
