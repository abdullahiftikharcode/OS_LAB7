## Multi-threaded File Storage Server - Code Walkthrough

This document explains the entire codebase line-by-line, in execution flow order. Code excerpts reference real files and line numbers from this project.

### Makefile

```1:18:Makefile
CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -std=c11 -O2 -pthread
LDFLAGS=-pthread

SRC=src/main.c src/queue.c src/user.c src/fs.c src/client.c src/worker.c
INC=-Iinclude

BIN=server

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)

.PHONY: all clean
```
- CC/CFLAGS/LDFLAGS: compiler and flags; uses pthreads.
- SRC: sources to compile.
- INC: adds `include/` to header search path.
- BIN: output executable name.
- `all` target builds server.
- Link step compiles sources with flags.
- `clean` removes binary.
- `.PHONY` declares non-file targets.

### Thread-safe Queue API

```1:29:include/queue.h
#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>

typedef struct QueueNode {
	void *data;
	struct QueueNode *next;
} QueueNode;

typedef struct Queue {
	QueueNode *head;
	QueueNode *tail;
	unsigned size;
	bool closed;
	pthread_mutex_t mutex;
	pthread_cond_t not_empty;
} Queue;

void queue_init(Queue *q);
void queue_close(Queue *q);
void queue_destroy(Queue *q);
bool queue_push(Queue *q, void *item);
// Returns NULL if closed and empty
void *queue_pop(Queue *q);
unsigned queue_size(Queue *q);

#endif
```
- Include guards prevent double inclusion.
- Includes pthreads and bool.
- `QueueNode`: singly linked list node storing generic `void*`.
- `Queue`: head/tail pointers, size counter, `closed` flag, mutex and condvar.
- Functions: init/close/destroy, push/pop (blocking), size.

```1:75:src/queue.c
#include "queue.h"
#include <stdlib.h>

void queue_init(Queue *q) {
	q->head = q->tail = NULL;
	q->size = 0;
	q->closed = false;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->not_empty, NULL);
}

void queue_close(Queue *q) {
	pthread_mutex_lock(&q->mutex);
	q->closed = true;
	pthread_cond_broadcast(&q->not_empty);
	pthread_mutex_unlock(&q->mutex);
}

void queue_destroy(Queue *q) {
	QueueNode *n = q->head;
	while (n) {
		QueueNode *next = n->next;
		free(n);
		n = next;
	}
	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->not_empty);
}

bool queue_push(Queue *q, void *item) {
	pthread_mutex_lock(&q->mutex);
	if (q->closed) {
		pthread_mutex_unlock(&q->mutex);
		return false;
	}
	QueueNode *n = (QueueNode *)malloc(sizeof(QueueNode));
	if (!n) {
		pthread_mutex_unlock(&q->mutex);
		return false;
	}
	n->data = item;
	n->next = NULL;
	if (q->tail) q->tail->next = n; else q->head = n;
	q->tail = n;
	q->size++;
	pthread_cond_signal(&q->not_empty);
	pthread_mutex_unlock(&q->mutex);
	return true;
}

void *queue_pop(Queue *q) {
	pthread_mutex_lock(&q->mutex);
	while (!q->head && !q->closed) {
		pthread_cond_wait(&q->not_empty, &q->mutex);
	}
	if (!q->head) {
		pthread_mutex_unlock(&q->mutex);
		return NULL;
	}
	QueueNode *n = q->head;
	q->head = n->next;
	if (!q->head) q->tail = NULL;
	q->size--;
	void *data = n->data;
	free(n);
	pthread_mutex_unlock(&q->mutex);
	return data;
}

unsigned queue_size(Queue *q) {
	pthread_mutex_lock(&q->mutex);
	unsigned s = q->size;
	pthread_mutex_unlock(&q->mutex);
	return s;
}
```
- `queue_init`: zeroes pointers/counters; initializes mutex/condvar.
- `queue_close`: marks closed; wakes all waiters so `queue_pop` can return NULL.
- `queue_destroy`: frees remaining nodes; destroys sync primitives.
- `queue_push`: lock, guard closed, allocate node, append to tail (or set head), inc size, signal waiters, unlock.
- `queue_pop`: lock, wait while empty and not closed, if empty+closed return NULL, remove head, adjust tail, dec size, return data.
- `queue_size`: returns size under mutex.

### Core Types

```1:45:include/types.h
#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
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

typedef struct {
	CommandType type;
	ClientInfo client;
	char username[64];
	char password[64];
	char path[256];
	char tmpfile[256]; // for uploads
	size_t size;
} Task;

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
```
- `CommandType`: supported operations.
- `ClientInfo`: connection identity + fd.
- `Task`: unit of work filled by client threads for workers.
- `ResponseStatus` and `Response`: what workers produce for client threads.

### User Store (struct-based design)

```1:36:include/user.h
#ifndef USER_H
#define USER_H

#include <pthread.h>
#include <stdbool.h>

typedef struct User {
	char name[64];
	char pass[64];
	pthread_mutex_t mutex; // per-user lock
	size_t used_bytes;
	size_t quota_bytes;
} User;

typedef struct UserNode {
	User user;
	struct UserNode *next;
} UserNode;

typedef struct UserStore {
	UserNode *head;
	pthread_mutex_t users_mutex;
	char storage_root[256];
	size_t user_count;
} UserStore;

// API functions
UserStore* user_store_create(const char *root);
void user_store_destroy(UserStore *store);
bool user_store_signup(UserStore *store, const char *name, const char *pass, size_t quota_bytes);
bool user_store_login(UserStore *store, const char *name, const char *pass);
User* user_store_lock_user(UserStore *store, const char *name);
void user_store_unlock_user(User *user);
const char* user_store_get_root(UserStore *store);

#endif
```
- Declares `User` with per-user mutex and quota fields.
- `UserNode`: singly linked list node for users.
- `UserStore`: encapsulates the user store with its own mutex and storage root.
- Clean API functions that take `UserStore*` parameter for proper encapsulation.

```1:118:src/user.c
#include "user.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

// Struct-based API
UserStore* user_store_create(const char *root) {
	UserStore *store = (UserStore *)calloc(1, sizeof(UserStore));
	if (!store) return NULL;
	
	if (root && *root) {
		strncpy(store->storage_root, root, sizeof(store->storage_root)-1);
		store->storage_root[sizeof(store->storage_root)-1] = '\0';
	}
	
	pthread_mutex_init(&store->users_mutex, NULL);
	mkdir(store->storage_root, 0777);
	return store;
}

void user_store_destroy(UserStore *store) {
	if (!store) return;
	
	pthread_mutex_lock(&store->users_mutex);
	UserNode *n = store->head;
	while (n) {
		UserNode *next = n->next;
		pthread_mutex_destroy(&n->user.mutex);
		free(n);
		n = next;
	}
	pthread_mutex_unlock(&store->users_mutex);
	pthread_mutex_destroy(&store->users_mutex);
	free(store);
}

bool user_store_signup(UserStore *store, const char *name, const char *pass, size_t quota_bytes) {
	pthread_mutex_lock(&store->users_mutex);
	
	// Check if user already exists
	UserNode *n = store->head;
	while (n) {
		if (strcmp(n->user.name, name) == 0) {
			pthread_mutex_unlock(&store->users_mutex);
			return false;
		}
		n = n->next;
	}
	
	// Create new user
	UserNode *node = (UserNode *)calloc(1, sizeof(UserNode));
	if (!node) {
		pthread_mutex_unlock(&store->users_mutex);
		return false;
	}
	
	strncpy(node->user.name, name, sizeof(node->user.name)-1);
	strncpy(node->user.pass, pass, sizeof(node->user.pass)-1);
	node->user.quota_bytes = quota_bytes;
	pthread_mutex_init(&node->user.mutex, NULL);
	
	node->next = store->head;
	store->head = node;
	store->user_count++;
	
	pthread_mutex_unlock(&store->users_mutex);
	
	// Create user directory
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", store->storage_root, name);
	mkdir(path, 0777);
	
	return true;
}

bool user_store_login(UserStore *store, const char *name, const char *pass) {
	pthread_mutex_lock(&store->users_mutex);
	
	UserNode *n = store->head;
	while (n) {
		if (strcmp(n->user.name, name) == 0) {
			bool ok = (strcmp(n->user.pass, pass) == 0);
			pthread_mutex_unlock(&store->users_mutex);
			return ok;
		}
		n = n->next;
	}
	
	pthread_mutex_unlock(&store->users_mutex);
	return false;
}

User* user_store_lock_user(UserStore *store, const char *name) {
	pthread_mutex_lock(&store->users_mutex);
	
	UserNode *n = store->head;
	while (n) {
		if (strcmp(n->user.name, name) == 0) {
			pthread_mutex_lock(&n->user.mutex);
			pthread_mutex_unlock(&store->users_mutex);
			return &n->user;
		}
		n = n->next;
	}
	
	pthread_mutex_unlock(&store->users_mutex);
	return NULL;
}

void user_store_unlock_user(User *user) {
	if (user) pthread_mutex_unlock(&user->mutex);
}

const char* user_store_get_root(UserStore *store) {
	return store ? store->storage_root : NULL;
}
```
- **Clean Design**: `UserStore` struct encapsulates all user store state.
- `user_store_create`: allocates and initializes new store instance.
- `user_store_destroy`: cleans up store and all users.
- `user_store_signup`: adds user to store's linked list, creates directory.
- `user_store_login`: validates credentials against store.
- `user_store_lock_user`/`user_store_unlock_user`: per-user locking.
- **No Global State**: Each UserStore instance is independent.

### Filesystem helpers

```1:14:include/fs.h
#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdbool.h>
#include "user.h"

bool fs_upload(UserStore *store, const char *user, const char *dst_relpath, const char *tmp_src, size_t size, char *err, size_t errlen);
bool fs_download_path(UserStore *store, const char *user, const char *relpath, char *out_path, size_t out_len, char *err, size_t errlen);
bool fs_delete(UserStore *store, const char *user, const char *relpath, char *err, size_t errlen);
// Writes listing into buffer separated by \n
bool fs_list(UserStore *store, const char *user, char *buf, size_t buflen, char *err, size_t errlen);

#endif
```
- Declares file ops APIs that take `UserStore*` parameter.

```1:62:src/fs.c
#include "fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

static void build_user_path(UserStore *store, const char *user, const char *rel, char *out, size_t out_len) {
	snprintf(out, out_len, "%s/%s/%s", user_store_get_root(store), user, rel ? rel : "");
}

bool fs_upload(UserStore *store, const char *user, const char *dst_relpath, const char *tmp_src, size_t size, char *err, size_t errlen) {
	char dst[512];
	build_user_path(store, user, dst_relpath, dst, sizeof(dst));
	FILE *in = fopen(tmp_src, "rb");
	if (!in) { snprintf(err, errlen, "open tmp_src failed"); return false; }
	FILE *out = fopen(dst, "wb");
	if (!out) { fclose(in); snprintf(err, errlen, "open dst failed"); return false; }
	char buf[8192];
	size_t total = 0, n;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		fwrite(buf, 1, n, out);
		total += n;
	}
	fclose(in);
	fclose(out);
	(void)size; // could enforce quota elsewhere
	return true;
}

bool fs_download_path(UserStore *store, const char *user, const char *relpath, char *out_path, size_t out_len, char *err, size_t errlen) {
	(void)err; (void)errlen;
	build_user_path(store, user, relpath, out_path, out_len);
	return true;
}

bool fs_delete(UserStore *store, const char *user, const char *relpath, char *err, size_t errlen) {
	char path[512];
	build_user_path(store, user, relpath, path, sizeof(path));
	if (remove(path) != 0) { snprintf(err, errlen, "remove failed"); return false; }
	return true;
}

bool fs_list(UserStore *store, const char *user, char *buf, size_t buflen, char *err, size_t errlen) {
	char path[512];
	build_user_path(store, user, NULL, path, sizeof(path));
	DIR *d = opendir(path);
	if (!d) { snprintf(err, errlen, "opendir failed"); return false; }
	struct dirent *de;
	size_t used = 0;
	while ((de = readdir(d))) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
		int w = snprintf(buf + used, buflen - used, "%s\n", de->d_name);
		if (w < 0 || (size_t)w >= buflen - used) break;
		used += (size_t)w;
	}
	closedir(d);
	return true;
}
```
- `build_user_path`: constructs `<root>/<user>/<rel>` using store's root.
- `fs_upload`: copy file from server-local `tmp_src` to destination.
- `fs_download_path`: just builds absolute path for now.
- `fs_delete`: removes file.
- `fs_list`: enumerates user directory, skipping `.` and `..`.

### Server-level types

```1:14:include/server.h
#ifndef SERVER_H
#define SERVER_H

#include "queue.h"
#include "types.h"

typedef struct ResponseQueueEntry {
	int client_id;
	Queue queue; // responses for this client
} ResponseQueueEntry;

void server_run(unsigned port, int client_threads, int worker_threads);

#endif
```
- `ResponseQueueEntry`: mapping from client_id to a per-client `Queue` of `Response*`.

### Client thread code

```1:16:include/client.h
#ifndef CLIENT_H
#define CLIENT_H

#include "queue.h"
#include "types.h"

typedef struct {
	int socket_fd;
	int client_id;
	Queue *client_queue; // queue of ClientInfo pointers
} ClientThreadArg;

void *client_thread_main(void *arg);

#endif
```
- `ClientThreadArg`: shared args for client pool threads (pointer to queue of pending `ClientInfo*`).

```1:84:src/client.c
#include "client.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
			extern Queue g_task_queue;
			queue_push(&g_task_queue, task);

			extern Queue g_response_map;
			ResponseQueueEntry *entry = NULL;
			QueueNode *scan;
			pthread_mutex_lock(&g_response_map.mutex);
			scan = g_response_map.head;
			while (scan) {
				ResponseQueueEntry *e = (ResponseQueueEntry *)scan->data;
				if (e->client_id == client_id) { entry = e; break; }
				scan = scan->next;
			}
			pthread_mutex_unlock(&g_response_map.mutex);
			if (!entry) break;
			Response *resp = (Response *)queue_pop(&entry->queue);
			if (!resp) break;
			char out[1024];
			snprintf(out, sizeof(out), "%s %s\n", resp->status==RESP_OK?"OK":"ERR", resp->message);
			write(fd, out, strlen(out));
			free(resp);
		}
		// deregister response queue for this client
		extern void deregister_client_response_queue(int client_id);
		close(fd);
		deregister_client_response_queue(client_id);
	}
	return NULL;
}
```
- `recv_line`: blocking read of a line into buffer, null-terminates.
- Outer loop: keeps fetching new connections from the Client Queue.
- Inner loop: for a connection, parse commands and build a `Task`.
- After pushing task, it looks up its `ResponseQueueEntry`, waits for response, writes back.
- On disconnect/QUIT: closes fd and deregisters the response queue.

### Worker thread code

```1:80:src/worker.c
#include "worker.h"
#include "server.h"
#include "fs.h"
#include "user.h"
#include <stdlib.h>
#include <string.h>

static void send_response(Queue *resp_map, int client_id, ResponseStatus st, const char *msg) {
	Response *r = (Response *)calloc(1, sizeof(Response));
	r->client_id = client_id;
	r->status = st;
	strncpy(r->message, msg?msg:"", sizeof(r->message)-1);
	ResponseQueueEntry *entry = NULL;
	QueueNode *scan;
	pthread_mutex_lock(&resp_map->mutex);
	scan = resp_map->head;
	while (scan) {
		ResponseQueueEntry *e = (ResponseQueueEntry *)scan->data;
		if (e->client_id == client_id) { entry = e; break; }
		scan = scan->next;
	}
	pthread_mutex_unlock(&resp_map->mutex);
	if (entry) queue_push(&entry->queue, r); else free(r);
}

void *worker_thread_main(void *arg) {
	WorkerPoolArg *wpa = (WorkerPoolArg *)arg;
	while (1) {
		Task *t = (Task *)queue_pop(wpa->task_queue);
		if (!t) break;
		char err[256] = {0};
		switch (t->type) {
			case CMD_SIGNUP: {
				bool ok = user_signup(t->username, t->password, 100<<20);
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?"signed_up":"signup_failed");
				break;
			}
			case CMD_LOGIN: {
				bool ok = user_login(t->username, t->password);
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?"logged_in":"login_failed");
				break;
			}
			case CMD_UPLOAD: {
				User *u = user_lock(t->username);
				bool ok = false;
				if (u) {
					ok = fs_upload(t->username, t->path, t->tmpfile, t->size, err, sizeof(err));
					user_unlock(u);
				}
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?"uploaded":err);
				break;
			}
			case CMD_DOWNLOAD: {
				char p[512];
				bool ok = fs_download_path(t->username, t->path, p, sizeof(p), err, sizeof(err));
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?p:err);
				break;
			}
			case CMD_DELETE: {
				User *u = user_lock(t->username);
				bool ok = false;
				if (u) { ok = fs_delete(t->username, t->path, err, sizeof(err)); user_unlock(u);} 
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?"deleted":err);
				break;
			}
			case CMD_LIST: {
				char buf[1024];
				bool ok = fs_list(t->username, buf, sizeof(buf), err, sizeof(err));
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?buf:err);
				break;
			}
			default: break;
		}
		free(t);
	}
	return NULL;
}
```
- `send_response`: allocates `Response`, finds client’s response queue, enqueues it.
- Worker loop: blocks on `g_task_queue`, processes commands, uses per-user locks for FS ops, enqueues `Response`, frees `Task`.

### Main: accept, threadpools, shutdown

```1:140:src/main.c
#include "server.h"
#include "client.h"
#include "worker.h"
#include "user.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef struct ServerState {
	Queue task_queue;
	Queue response_map;
	UserStore *user_store;
	int listen_fd;
	volatile int running;
} ServerState;

static ServerState *g_server = NULL;

static void on_sigint(int s) { 
	(void)s; 
	if (g_server) {
		g_server->running = 0; 
		if (g_server->listen_fd >= 0) close(g_server->listen_fd); 
		queue_close(&g_server->task_queue); 
	}
}

static int tcp_listen(unsigned short port) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	int opt=1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET; addr.sin_port = htons(port); addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	listen(fd, 64);
	return fd;
}

static ResponseQueueEntry *register_client_response_queue(ServerState *server, int client_id) {
	ResponseQueueEntry *e = (ResponseQueueEntry *)calloc(1, sizeof(ResponseQueueEntry));
	e->client_id = client_id;
	queue_init(&e->queue);
	queue_push(&server->response_map, e);
	return e;
}

// remove entry from response map
void deregister_client_response_queue(ServerState *server, int client_id) {
    pthread_mutex_lock(&server->response_map.mutex);
    QueueNode *prev = NULL, *cur = server->response_map.head;
    while (cur) {
        ResponseQueueEntry *e = (ResponseQueueEntry *)cur->data;
        if (e->client_id == client_id) {
            if (prev) prev->next = cur->next; else server->response_map.head = cur->next;
            if (server->response_map.tail == cur) server->response_map.tail = prev;
            server->response_map.size--;
            pthread_mutex_unlock(&server->response_map.mutex);
            queue_close(&e->queue);
            queue_destroy(&e->queue);
            free(e);
            free(cur);
            return;
        }
        prev = cur; cur = cur->next;
    }
    pthread_mutex_unlock(&server->response_map.mutex);
}

int main(int argc, char **argv) {
	unsigned short port = 9090;
	int client_threads = 4;
	int worker_threads = 4;
	if (argc > 1) port = (unsigned short)atoi(argv[1]);
	
	// Create server state
	ServerState *server = (ServerState *)calloc(1, sizeof(ServerState));
	if (!server) {
		fprintf(stderr, "Failed to allocate server state\n");
		return 1;
	}
	
	server->user_store = user_store_create("storage");
	if (!server->user_store) {
		fprintf(stderr, "Failed to create user store\n");
		free(server);
		return 1;
	}
	
	queue_init(&server->task_queue);
	queue_init(&server->response_map);
	server->running = 1;
	g_server = server;
	signal(SIGINT, on_sigint);

    int lfd = tcp_listen(port);
    server->listen_fd = lfd;
    printf("Server listening on %u\n", port);

	// launch worker threads
	pthread_t *wts = calloc((size_t)worker_threads, sizeof(pthread_t));
	WorkerPoolArg wpa = { .task_queue=&server->task_queue, .resp_queues=&server->response_map, .user_store=server->user_store };
	for (int i=0;i<worker_threads;i++) pthread_create(&wts[i], NULL, worker_thread_main, &wpa);

	Queue client_queue; queue_init(&client_queue);
	pthread_t *cts = calloc((size_t)client_threads, sizeof(pthread_t));
	ClientThreadArg cta = { .client_queue=&client_queue };
	for (int i=0;i<client_threads;i++) pthread_create(&cts[i], NULL, client_thread_main, &cta);

	int next_client_id = 1;
    while (server->running) {
		struct sockaddr_in cli; socklen_t cl = sizeof(cli);
		int cfd = accept(lfd, (struct sockaddr*)&cli, &cl);
        if (cfd < 0) {
            if (!server->running) break; else continue;
        }
		cta.client_id = next_client_id;
		register_client_response_queue(server, next_client_id);
		ClientInfo *ci = (ClientInfo *)calloc(1, sizeof(ClientInfo));
		ci->socket_fd = cfd; ci->client_id = next_client_id;
		next_client_id++;
		queue_push(&client_queue, ci);
	}

	queue_close(&client_queue);
	queue_close(&server->task_queue);
	queue_close(&server->response_map);
	for (int i=0;i<client_threads;i++) pthread_join(cts[i], NULL);
	free(cts);
	for (int i=0;i<worker_threads;i++) pthread_join(wts[i], NULL);
	free(wts);
	user_store_destroy(server->user_store);
	free(server);
	return 0;
}
```
- **ServerState**: encapsulates all server state (queues, user store, listen fd, running flag).
- **g_server**: global pointer to server state for signal handler access.
- **on_sigint**: graceful stop—close listen socket, close task queue to wake workers.
- **tcp_listen**: create/bind/listen TCP server socket.
- **register_client_response_queue**: allocate entry for new client and push into map.
- **deregister_client_response_queue**: remove client's entry, close/destroy its response queue.
- **main**: parse port; create server state with UserStore; init queues; set SIGINT handler; start worker pool with UserStore; start client pool; accept loop creates `ClientInfo` per connection, registers a response queue, and enqueues to client queue; on shutdown, close queues, join threads, destroy UserStore, and free server state.

---

That covers every line of code with how it participates in the execution flow: accept → client threads parse → enqueue task → worker executes under correct locks → enqueue response → client writes back → cleanup and graceful shutdown.


