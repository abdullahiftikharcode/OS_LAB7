#include "queue.h"
#include "types.h"
#include "task.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

// Helper function to swap two priority items
static void swap_items(PriorityItem *a, PriorityItem *b) {
    PriorityItem temp = *a;
    *a = *b;
    *b = temp;
}

// Helper function to maintain heap property using task_compare_priority
static void heapify_up(Queue *q, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        Task *parent_task = (Task *)q->items[parent].data;
        Task *current_task = (Task *)q->items[index].data;
        
        // Use task_compare_priority for consistent comparison
        if (task_compare_priority(&parent_task, &current_task) >= 0) {
            break;
        }
        swap_items(&q->items[parent], &q->items[index]);
        index = parent;
    }
}

// Helper function to maintain heap property (downward) using task_compare_priority
static void heapify_down(Queue *q, size_t index) {
    while (1) {
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;
        size_t largest = index;

        if (left < q->size) {
            Task *left_task = (Task *)q->items[left].data;
            Task *largest_task = (Task *)q->items[largest].data;
            if (task_compare_priority(&left_task, &largest_task) > 0) {
                largest = left;
            }
        }
        
        if (right < q->size) {
            Task *right_task = (Task *)q->items[right].data;
            Task *largest_task = (Task *)q->items[largest].data;
            if (task_compare_priority(&right_task, &largest_task) > 0) {
                largest = right;
            }
        }
        
        if (largest == index) {
            break;
        }
        
        swap_items(&q->items[index], &q->items[largest]);
        index = largest;
    }
}

void queue_init(Queue *q) {
    q->items = NULL;
    q->size = 0;
    q->capacity = 0;
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
    if (q->items) {
        free(q->items);
        q->items = NULL;
    }
    q->size = 0;
    q->capacity = 0;
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
}

bool queue_push(Queue *q, void *item, TaskPriority priority) {
    printf("[queue_push] Attempting to lock mutex %p (queue=%p)...\n", 
           (void*)&q->mutex, (void*)q);
    fflush(stdout);
    pthread_mutex_lock(&q->mutex);
    printf("[queue_push] Mutex %p locked!\n", (void*)&q->mutex);
    fflush(stdout);
    
    if (q->closed) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }
    
    // Resize the array if needed
    if (q->size >= q->capacity) {
        size_t new_capacity = (q->capacity == 0) ? 16 : q->capacity * 2;
        PriorityItem *new_items = (PriorityItem *)realloc(q->items, new_capacity * sizeof(PriorityItem));
        if (!new_items) {
            pthread_mutex_unlock(&q->mutex);
            return false;
        }
        q->items = new_items;
        q->capacity = new_capacity;
    }
    
    // Add the new item
    q->items[q->size].data = item;
    q->items[q->size].priority = priority;
    q->size++;
    
    // Maintain heap property
    heapify_up(q, q->size - 1);
    
    // Signal that the queue is not empty
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

void *queue_pop(Queue *q) {
    printf("[queue_pop] Thread %lu attempting to lock mutex %p (queue=%p)...\n", 
           (unsigned long)pthread_self(), (void*)&q->mutex, (void*)q);
    fflush(stdout);
    pthread_mutex_lock(&q->mutex);
    printf("[queue_pop] Thread %lu mutex %p locked! size=%zu, closed=%d\n", 
           (unsigned long)pthread_self(), (void*)&q->mutex, q->size, q->closed);
    fflush(stdout);
    
    // Wait until there's an item or the queue is closed
    while (q->size == 0 && !q->closed) {
        printf("[queue_pop] Thread %lu waiting on condition variable...\n", (unsigned long)pthread_self());
        fflush(stdout);
        pthread_cond_wait(&q->not_empty, &q->mutex);
        printf("[queue_pop] Thread %lu woke up from condition variable! size=%zu\n", 
               (unsigned long)pthread_self(), q->size);
        fflush(stdout);
    }
    
    // If queue is empty and closed, return NULL
    if (q->size == 0) {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
    
    // Get the highest priority item (at index 0)
    void *data = q->items[0].data;
    
    // Move the last item to the root
    q->items[0] = q->items[--q->size];
    
    // Maintain heap property
    if (q->size > 0) {
        heapify_down(q, 0);
    }
    
    pthread_mutex_unlock(&q->mutex);
    return data;
}

unsigned queue_size(Queue *q) {
    pthread_mutex_lock(&q->mutex);
    unsigned size = (unsigned)q->size;
    pthread_mutex_unlock(&q->mutex);
    return size;
}

void *queue_peek(Queue *q) {
    pthread_mutex_lock(&q->mutex);
    void *data = (q->size > 0) ? q->items[0].data : NULL;
    pthread_mutex_unlock(&q->mutex);
    return data;
}
