#include "server.h"
#include "client.h"
#include "worker.h"
#include "user.h"
#include "config.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <libgen.h>

ServerState *g_server = NULL;

static void on_sigint(int s) { 
	(void)s; 
	if (g_server) {
		g_server->running = 0;
		// Close listen socket to unblock accept()
		if (g_server->listen_fd >= 0) {
			shutdown(g_server->listen_fd, SHUT_RDWR);
			close(g_server->listen_fd);
		}
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
    if (!e) return NULL;
    
    e->client_id = client_id;
    queue_init(&e->queue);
    pthread_mutex_init(&e->mutex, NULL);
    pthread_cond_init(&e->response_available, NULL);
    
    // Add to response map (queue_push handles its own locking)
    bool success = queue_push(&server->response_map, e, PRIORITY_NORMAL);
    
    if (!success) {
        pthread_cond_destroy(&e->response_available);
        pthread_mutex_destroy(&e->mutex);
        queue_destroy(&e->queue);
        free(e);
        return NULL;
    }
    
    return e;
}

// remove entry from response map
void deregister_client_response_queue(ServerState *server, int client_id) {
    pthread_mutex_lock(&server->response_map.mutex);
    
    // Search through the priority queue items
    ResponseQueueEntry *found = NULL;
    size_t found_idx = 0;
    for (size_t i = 0; i < server->response_map.size; i++) {
        ResponseQueueEntry *e = (ResponseQueueEntry *)server->response_map.items[i].data;
        if (e->client_id == client_id) {
            found = e;
            found_idx = i;
            break;
        }
    }
    
    if (found) {
        // Remove from queue by replacing with last element and reducing size
        if (found_idx < server->response_map.size - 1) {
            server->response_map.items[found_idx] = server->response_map.items[server->response_map.size - 1];
        }
        server->response_map.size--;
        pthread_mutex_unlock(&server->response_map.mutex);
        
        // Clean up the entry
        pthread_mutex_lock(&found->mutex);
        queue_close(&found->queue);
        pthread_cond_broadcast(&found->response_available);  // Wake up any waiting threads
        pthread_mutex_unlock(&found->mutex);
        
        // Destroy resources
        queue_destroy(&found->queue);
        pthread_cond_destroy(&found->response_available);
        pthread_mutex_destroy(&found->mutex);
        free(found);
    } else {
        pthread_mutex_unlock(&server->response_map.mutex);
    }
}

int main(int argc, char **argv) {
	unsigned short port = 9090;
	int client_threads = 4;
	int worker_threads = 4;
	// Parse command line arguments
	char *config_file = "config.ini";
	if (argc > 1) port = (unsigned short)atoi(argv[1]);
	if (argc > 2) config_file = argv[2];
	
	// Initialize configuration
	config_init(config_file);
	
	// Get storage path from config
	const char *storage_path = config_get_storage_path();
	
	// Create server state
	ServerState *server = (ServerState *)calloc(1, sizeof(ServerState));
	if (!server) {
		fprintf(stderr, "Failed to allocate server state\n");
		return 1;
	}
	
	server->user_store = user_store_create(storage_path);
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
		printf("[Main] Accepted connection (fd=%d, client_id=%d)\n", cfd, next_client_id);
		fflush(stdout);
		cta.client_id = next_client_id;
		register_client_response_queue(server, next_client_id);
		ClientInfo *ci = (ClientInfo *)calloc(1, sizeof(ClientInfo));
		ci->socket_fd = cfd; ci->client_id = next_client_id;
		next_client_id++;
		printf("[Main] About to push to client_queue at %p\n", (void*)&client_queue);
		fflush(stdout);
		queue_push(&client_queue, ci, PRIORITY_NORMAL);
		printf("[Main] Pushed client to queue (client_id=%d)\n", next_client_id - 1);
		fflush(stdout);
	}

	// Shutdown sequence: close queues to wake up waiting threads
	queue_close(&client_queue);
	queue_close(&server->task_queue);
	queue_close(&server->response_map);
	for (int i=0;i<client_threads;i++) pthread_join(cts[i], NULL);
	free(cts);
	for (int i=0;i<worker_threads;i++) pthread_join(wts[i], NULL);
	free(wts);
	
	// Clean up configuration
	// Note: config_cleanup() would be called here if we had one
	
	// Clean up remaining response queue entries
	pthread_mutex_lock(&server->response_map.mutex);
	for (size_t i = 0; i < server->response_map.size; i++) {
		ResponseQueueEntry *e = (ResponseQueueEntry *)server->response_map.items[i].data;
		if (e) {
			queue_destroy(&e->queue);
			pthread_cond_destroy(&e->response_available);
			pthread_mutex_destroy(&e->mutex);
			free(e);
		}
	}
	pthread_mutex_unlock(&server->response_map.mutex);
	
	// Destroy queues
	queue_destroy(&client_queue);
	queue_destroy(&server->task_queue);
	queue_destroy(&server->response_map);
	
	user_store_destroy(server->user_store);
	free(server);
	return 0;
}


