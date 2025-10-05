#include "worker.h"
#include "server.h"
#include "fs.h"
#include "user.h"
#include <stdlib.h>
#include <string.h>
<<<<<<< HEAD
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>

// Forward declarations
static void send_response(Queue *resp_map, int client_id, ResponseStatus st, const char *msg);
static void send_file_data(int socket_fd, const char *file_path, const char *filename);
=======

// Forward declaration
static void send_response(Queue *resp_map, int client_id, ResponseStatus st, const char *msg);
>>>>>>> 46fe509ab8b271fddaa8c1943bb7d1d8624904bb

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

<<<<<<< HEAD
static void send_file_data(int socket_fd, const char *file_path, const char *filename) {
	FILE *file = fopen(file_path, "rb");
	if (!file) return;
	
	// Get file size
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	// Send file info header
	char header[512];
	snprintf(header, sizeof(header), "FILE_DATA %s %ld\n", filename, file_size);
	write(socket_fd, header, strlen(header));
	
	// Send file data in chunks
	char buffer[8192];
	size_t bytes_read;
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		write(socket_fd, buffer, bytes_read);
	}
	
	fclose(file);
}

=======
>>>>>>> 46fe509ab8b271fddaa8c1943bb7d1d8624904bb
void *worker_thread_main(void *arg) {
	WorkerPoolArg *wpa = (WorkerPoolArg *)arg;
	while (1) {
		Task *t = (Task *)queue_pop(wpa->task_queue);
		if (!t) break;
		char err[256] = {0};
		switch (t->type) {
			case CMD_SIGNUP: {
				bool ok = user_store_signup(wpa->user_store, t->username, t->password, 100<<20);
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?"signed_up":"signup_failed");
				break;
			}
			case CMD_LOGIN: {
				bool ok = user_store_login(wpa->user_store, t->username, t->password);
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?"logged_in":"login_failed");
				break;
			}
			case CMD_UPLOAD: {
				User *u = user_store_lock_user(wpa->user_store, t->username);
				bool ok = false;
				if (u) {
					ok = fs_upload(wpa->user_store, t->username, t->path, t->tmpfile, t->size, err, sizeof(err));
					user_store_unlock_user(u);
				}
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?"uploaded":err);
				break;
			}
			case CMD_DOWNLOAD: {
				char p[512];
				bool ok = fs_download_path(wpa->user_store, t->username, t->path, p, sizeof(p), err, sizeof(err));
<<<<<<< HEAD
				if (ok) {
					// Send response first, then file data
					send_response(wpa->resp_queues, t->client.client_id, RESP_OK, "downloaded");
					// Send file data through socket
					send_file_data(t->client.socket_fd, p, t->path);
					// Clean up temp file
					unlink(p);
				} else {
					send_response(wpa->resp_queues, t->client.client_id, RESP_ERR, err);
				}
=======
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?p:err);
>>>>>>> 46fe509ab8b271fddaa8c1943bb7d1d8624904bb
				break;
			}
			case CMD_DELETE: {
				User *u = user_store_lock_user(wpa->user_store, t->username);
				bool ok = false;
				if (u) { ok = fs_delete(wpa->user_store, t->username, t->path, err, sizeof(err)); user_store_unlock_user(u);} 
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?"deleted":err);
				break;
			}
			case CMD_LIST: {
				char buf[1024];
				bool ok = fs_list(wpa->user_store, t->username, buf, sizeof(buf), err, sizeof(err));
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?buf:err);
				break;
			}
			default: break;
		}
		free(t);
	}
	return NULL;
}


