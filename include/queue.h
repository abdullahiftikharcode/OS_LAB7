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

