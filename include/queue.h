#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include "types.h"

// Priority queue node
typedef struct {
    void *data;
    TaskPriority priority;
} PriorityItem;

typedef struct Queue {
    PriorityItem *items;  // Array to store heap elements
    size_t size;         // Current number of elements
    size_t capacity;     // Maximum capacity of the queue
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} Queue;

typedef struct ResponseQueueEntry {
	int client_id;
	Queue queue;
	pthread_mutex_t mutex;
	pthread_cond_t response_available;
} ResponseQueueEntry;

// Initialize a priority queue
void queue_init(Queue *q);

// Close the queue (no more items can be added)
void queue_close(Queue *q);

// Destroy the queue and free resources
void queue_destroy(Queue *q);

// Push an item with priority into the queue
// Returns true on success, false on failure
bool queue_push(Queue *q, void *item, TaskPriority priority);

// Pop the highest priority item from the queue
// Returns NULL if queue is closed and empty
void *queue_pop(Queue *q);

// Get the current size of the queue
unsigned queue_size(Queue *q);

// Peek at the highest priority item without removing it
// Returns NULL if queue is empty
void *queue_peek(Queue *q);

#endif

