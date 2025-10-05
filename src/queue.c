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


