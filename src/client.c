#include "client.h"
#include "server.h"
#include "user.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

extern ServerState *g_server;

// Simple line-based protocol for demo purposes
static int recv_line(int fd, char *buf, size_t len) {
	size_t pos = 0;
	while (pos < len - 1) {
		char c;
		ssize_t n = read(fd, &c, 1);
		if (n <= 0) return (pos > 0) ? (int)pos : -1;
		if (c == '\n') {
			buf[pos] = '\0';
			return (int)pos;
		}
		buf[pos++] = c;
	}
	buf[pos] = '\0';
	return (int)pos;
}

void *client_thread_main(void *arg) {
    ClientThreadArg *cta = (ClientThreadArg *)arg;
    char line[512];
    while (1) {
        printf("[Client Thread %lu] Waiting for client...\n", (unsigned long)pthread_self());
        fflush(stdout);
        ClientInfo *ci = (ClientInfo *)queue_pop(cta->client_queue);
        if (!ci) break;
        int fd = ci->socket_fd;
        int client_id = ci->client_id;
        printf("[Client Thread %lu] Got client (fd=%d, client_id=%d)\n", 
               (unsigned long)pthread_self(), fd, client_id);
        fflush(stdout);
        free(ci);
        while (1) {
            int r = recv_line(fd, line, sizeof(line));
            if (r <= 0) break;
            
            printf("[Client] Received command: %s\n", line);
            
            // Default to normal priority
            TaskPriority priority = PRIORITY_NORMAL;
            
            // Create task with default priority
            Task *task = task_create(CMD_UNKNOWN, (ClientInfo){client_id, fd}, 
                                   "", "", "", "", 0, priority);
            if (!task) {
                // Handle allocation failure
                char err_msg[] = "Error: Failed to allocate task\n";
                write(fd, err_msg, sizeof(err_msg) - 1);
                continue;
            }
            // Parse command and set appropriate priority
            if (strncmp(line, "SIGNUP", 6) == 0) {
                task->type = CMD_SIGNUP;
                char priority_str[16] = {0};
                int parsed = sscanf(line+6, "%63s %63s %15s", task->username, task->password, priority_str);
                
                // Set priority based on optional third parameter
                if (parsed >= 3) {
                    if (strcasecmp(priority_str, "HIGH") == 0) {
                        task->priority = PRIORITY_HIGH;
                    } else if (strcasecmp(priority_str, "LOW") == 0) {
                        task->priority = PRIORITY_LOW;
                    } else {
                        task->priority = PRIORITY_NORMAL;
                    }
                } else {
                    // Default to NORMAL priority if not specified
                    task->priority = PRIORITY_NORMAL;
                }
            } else if (strncmp(line, "LOGIN", 5) == 0) {
                task->type = CMD_LOGIN;
                sscanf(line+5, "%63s %63s", task->username, task->password);
                // Login is a high-priority operation
                task->priority = PRIORITY_HIGH;
            } else if (strncmp(line, "UPLOAD", 6) == 0) {
                task->type = CMD_UPLOAD;
                sscanf(line+6, "%63s %255s %zu %255s", task->username, task->path, &task->size, task->tmpfile);
                // Upload can be normal priority
                task->priority = PRIORITY_NORMAL;
            } else if (strncmp(line, "DOWNLOAD", 8) == 0) {
                task->type = CMD_DOWNLOAD;
                sscanf(line+8, "%63s %255s", task->username, task->path);
                // Download can be normal priority
                task->priority = PRIORITY_NORMAL;
            } else if (strncmp(line, "DELETE", 6) == 0) {
                task->type = CMD_DELETE;
                sscanf(line+6, "%63s %255s", task->username, task->path);
                // Delete is a high-priority operation
                task->priority = PRIORITY_HIGH;
            } else if (strncmp(line, "LIST", 4) == 0) {
                task->type = CMD_LIST;
                sscanf(line+4, "%63s", task->username);
                // List is a low-priority operation
                task->priority = PRIORITY_LOW;
            } else if (strncmp(line, "QUIT", 4) == 0) {
                free(task);
                break;
            } else {
                free(task);
                continue;
            }
            // Push the task to the worker queue
            // Push task with its priority
            printf("[Client] Pushing task: %s (priority: %s, user: %s)\n", 
                   command_to_string(task->type), 
                   priority_to_string(task->priority),
                   task->username);
            queue_push(&g_server->task_queue, task, task->priority);
            
            // Log the task submission
            printf("[Client] Task submitted: %s (priority: %s)\n", 
                   command_to_string(task->type), 
                   priority_to_string(task->priority));

            // Find our response queue entry
            ResponseQueueEntry *entry = NULL;
            pthread_mutex_lock(&g_server->response_map.mutex);
            for (size_t i = 0; i < g_server->response_map.size; i++) {
                ResponseQueueEntry *e = (ResponseQueueEntry *)g_server->response_map.items[i].data;
                if (e->client_id == client_id) { 
                    entry = e; 
                    break; 
                }
            }
            pthread_mutex_unlock(&g_server->response_map.mutex);
            
            if (!entry) break;  // Client queue not found, connection lost

            // Initialize response to NULL
            Response *resp = NULL;
            
            // Wait for a response with a timeout
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 5;  // 5 second timeout

            pthread_mutex_lock(&entry->mutex);
            
            // Wait for a response with proper error handling
            int rc = 0;
            while (entry->queue.size == 0 && rc != ETIMEDOUT) {
                rc = pthread_cond_timedwait(&entry->response_available, &entry->mutex, &ts);
                if (rc != 0 && rc != ETIMEDOUT) {
                    // Handle other errors (like EINTR)
                    if (rc == EINTR) {
                        // If we were interrupted by a signal, recalculate the timeout
                        struct timespec now;
                        clock_gettime(CLOCK_REALTIME, &now);
                        if (now.tv_sec >= ts.tv_sec && 
                            (now.tv_sec > ts.tv_sec || now.tv_nsec >= ts.tv_nsec)) {
                            rc = ETIMEDOUT;
                        }
                    } else {
                        // For other errors, log and break
                        perror("pthread_cond_timedwait");
                        break;
                    }
                }
            }
            
            // Get the response if available
            if (entry->queue.size > 0) {
                resp = (Response *)queue_pop(&entry->queue);
            }
            pthread_mutex_unlock(&entry->mutex);
            
            // If we timed out, set an appropriate error message
            if (!resp && rc == ETIMEDOUT) {
                resp = (Response *)calloc(1, sizeof(Response));
                if (resp) {
                    resp->status = RESP_ERR;
                    strncpy(resp->message, "Request timed out", sizeof(resp->message) - 1);
                }
            }

            if (resp) {
                char out[1024];
                snprintf(out, sizeof(out), "%s %s\n", 
                        resp->status==RESP_OK?"OK":"ERR", 
                        resp->message);
                write(fd, out, strlen(out));
                free(resp);
            }
        }
        // deregister response queue for this client
        extern void deregister_client_response_queue(ServerState *server, int client_id);
        close(fd);
        deregister_client_response_queue(g_server, client_id);
    }
    return NULL;
}


