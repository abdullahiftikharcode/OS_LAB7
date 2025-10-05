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

