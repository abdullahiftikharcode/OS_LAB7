#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "priority.h"

typedef enum {
	CMD_UNKNOWN = 0,
	CMD_UPLOAD,
	CMD_DOWNLOAD,
	CMD_DELETE,
	CMD_LIST,
	CMD_SIGNUP,
	CMD_LOGIN,
	CMD_QUIT
} CommandType;

typedef struct {
	int client_id; // internal id for routing responses
	int socket_fd;
} ClientInfo;

typedef struct Task {
    CommandType type;
    ClientInfo client;
    char username[64];
    char password[64];
    char path[256];
    char tmpfile[256]; // for uploads
    size_t size;
    TaskPriority priority; // Task priority
    struct timespec enqueue_time; // When the task was added to the queue
} Task;

// Function to create a new task with priority
Task* task_create(CommandType type, ClientInfo client, const char *username, 
                 const char *password, const char *path, const char *tmpfile, 
                 size_t size, TaskPriority priority);

// Function to free a task
void task_free(Task *task);

// Function to compare two tasks for priority queue ordering
// Returns > 0 if a has higher priority than b
int task_compare_priority(const void *a, const void *b);

// Helper function to convert command type to string
const char *command_to_string(CommandType type);

typedef enum {
	RESP_OK=0,
	RESP_ERR=1
} ResponseStatus;

typedef struct {
	int client_id;
	ResponseStatus status;
	char message[512];
	char filepath[256]; // for download
	size_t size;
} Response;

#endif

