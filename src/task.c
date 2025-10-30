#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

Task* task_create(CommandType type, ClientInfo client, const char *username, 
                 const char *password, const char *path, const char *tmpfile, 
                 size_t size, TaskPriority priority) {
    Task *task = (Task*)calloc(1, sizeof(Task));
    if (!task) return NULL;
    
    task->type = type;
    task->client = client;
    task->size = size;
    task->priority = priority;
    
    // Initialize enqueue time to current time
    clock_gettime(CLOCK_REALTIME, &task->enqueue_time);
    
    // Copy strings safely
    if (username) strncpy(task->username, username, sizeof(task->username) - 1);
    if (password) strncpy(task->password, password, sizeof(task->password) - 1);
    if (path) strncpy(task->path, path, sizeof(task->path) - 1);
    if (tmpfile) strncpy(task->tmpfile, tmpfile, sizeof(task->tmpfile) - 1);
    
    return task;
}

void task_free(Task *task) {
    if (task) {
        free(task);
    }
}

int task_compare_priority(const void *a, const void *b) {
    const Task *task_a = *(const Task **)a;
    const Task *task_b = *(const Task **)b;
    
    // First compare by priority
    if (task_a->priority > task_b->priority) return -1;
    if (task_a->priority < task_b->priority) return 1;
    
    // If priorities are equal, compare by enqueue time (FIFO)
    if (task_a->enqueue_time.tv_sec < task_b->enqueue_time.tv_sec) return -1;
    if (task_a->enqueue_time.tv_sec > task_b->enqueue_time.tv_sec) return 1;
    if (task_a->enqueue_time.tv_nsec < task_b->enqueue_time.tv_nsec) return -1;
    if (task_a->enqueue_time.tv_nsec > task_b->enqueue_time.tv_nsec) return 1;
    
    return 0; // Equal priority and enqueue time
}

const char *command_to_string(CommandType type) {
    switch (type) {
        case CMD_UNKNOWN: return "UNKNOWN";
        case CMD_UPLOAD: return "UPLOAD";
        case CMD_DOWNLOAD: return "DOWNLOAD";
        case CMD_DELETE: return "DELETE";
        case CMD_LIST: return "LIST";
        case CMD_SIGNUP: return "SIGNUP";
        case CMD_LOGIN: return "LOGIN";
        case CMD_QUIT: return "QUIT";
        default: return "INVALID";
    }
}

void task_destroy(Task *task) {
    if (task) {
        free(task);
    }
}
