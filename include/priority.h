#ifndef PRIORITY_H
#define PRIORITY_H

typedef enum {
    PRIORITY_LOW = 0,    // Background/non-urgent tasks
    PRIORITY_NORMAL = 1,  // Regular user operations
    PRIORITY_HIGH = 2,    // Critical operations (admin tasks, etc.)
    PRIORITY_COUNT        // Keep this last
} TaskPriority;

// Get priority name as string (for logging/debugging)
const char *priority_to_string(TaskPriority priority);

#endif // PRIORITY_H
