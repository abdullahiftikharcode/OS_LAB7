#include "worker.h"
#include "server.h"
#include "fs.h"
#include "user.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration
static void send_response(Queue *resp_map, int client_id, ResponseStatus st, const char *msg);

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
				send_response(wpa->resp_queues, t->client.client_id, ok?RESP_OK:RESP_ERR, ok?p:err);
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


