#include "priority.h"

const char *priority_to_string(TaskPriority priority) {
    static const char *names[] = {
        [PRIORITY_LOW] = "LOW",
        [PRIORITY_NORMAL] = "NORMAL",
        [PRIORITY_HIGH] = "HIGH"
    };
    
    if (priority >= 0 && priority < PRIORITY_COUNT) {
        return names[priority];
    }
    return "UNKNOWN";
}
