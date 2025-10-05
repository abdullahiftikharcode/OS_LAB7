#ifndef WORKER_H
#define WORKER_H

#include "queue.h"
#include "types.h"
#include "user.h"

typedef struct {
	Queue *task_queue; // of Task*
	Queue *resp_queues; // mapping entries of ResponseQueueEntry*
	UserStore *user_store; // user store instance
} WorkerPoolArg;

void *worker_thread_main(void *arg);

#endif

