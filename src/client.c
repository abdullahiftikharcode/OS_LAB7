#include "client.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

extern ServerState *g_server;

// Simple line-based protocol for demo purposes
static int recv_line(int fd, char *buf, size_t len) {
	ssize_t n = read(fd, buf, len - 1);
	if (n <= 0) return -1;
	buf[n] = '\0';
	return (int)n;
}

void *client_thread_main(void *arg) {
    ClientThreadArg *cta = (ClientThreadArg *)arg;
    char line[512];
    while (1) {
        ClientInfo *ci = (ClientInfo *)queue_pop(cta->client_queue);
        if (!ci) break;
        int fd = ci->socket_fd;
        int client_id = ci->client_id;
        free(ci);
        while (1) {
            int r = recv_line(fd, line, sizeof(line));
            if (r <= 0) break;
            Task *task = (Task *)calloc(1, sizeof(Task));
            task->client.client_id = client_id;
            task->client.socket_fd = fd;
            if (strncmp(line, "SIGNUP", 6) == 0) {
                task->type = CMD_SIGNUP;
                sscanf(line+6, "%63s %63s", task->username, task->password);
            } else if (strncmp(line, "LOGIN", 5) == 0) {
                task->type = CMD_LOGIN;
                sscanf(line+5, "%63s %63s", task->username, task->password);
            } else if (strncmp(line, "UPLOAD", 6) == 0) {
                task->type = CMD_UPLOAD;
                sscanf(line+6, "%63s %255s %zu %255s", task->username, task->path, &task->size, task->tmpfile);
            } else if (strncmp(line, "DOWNLOAD", 8) == 0) {
                task->type = CMD_DOWNLOAD;
                sscanf(line+8, "%63s %255s", task->username, task->path);
            } else if (strncmp(line, "DELETE", 6) == 0) {
                task->type = CMD_DELETE;
                sscanf(line+6, "%63s %255s", task->username, task->path);
            } else if (strncmp(line, "LIST", 4) == 0) {
                task->type = CMD_LIST;
                sscanf(line+4, "%63s", task->username);
            } else if (strncmp(line, "QUIT", 4) == 0) {
                free(task);
                break;
            } else {
                free(task);
                continue;
            }
            queue_push(&g_server->task_queue, task);

            ResponseQueueEntry *entry = NULL;
            QueueNode *scan;
            pthread_mutex_lock(&g_server->response_map.mutex);
            scan = g_server->response_map.head;
            while (scan) {
                ResponseQueueEntry *e = (ResponseQueueEntry *)scan->data;
                if (e->client_id == client_id) { entry = e; break; }
                scan = scan->next;
            }
            pthread_mutex_unlock(&g_server->response_map.mutex);
            if (!entry) break;
            Response *resp = (Response *)queue_pop(&entry->queue);
            if (!resp) break;
            char out[1024];
            snprintf(out, sizeof(out), "%s %s\n", resp->status==RESP_OK?"OK":"ERR", resp->message);
            write(fd, out, strlen(out));
            free(resp);
        }
        // deregister response queue for this client
        extern void deregister_client_response_queue(ServerState *server, int client_id);
        close(fd);
        deregister_client_response_queue(g_server, client_id);
    }
    return NULL;
}


