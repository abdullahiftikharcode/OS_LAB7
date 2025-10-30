#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include "queue.h"
#include "types.h"
#include "user.h"

// ResponseQueueEntry is defined in queue.h

typedef struct ServerState {
	Queue task_queue;
	Queue response_map;
	UserStore *user_store;
	int listen_fd;
	volatile sig_atomic_t running;
} ServerState;

void server_run(unsigned port, int client_threads, int worker_threads);

#endif

