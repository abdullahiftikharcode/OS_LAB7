#include "worker.h"
#include "server.h"
#include "fs.h"
#include "user.h"
#include "priority.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

// Forward declarations
static void send_response(Queue *resp_map, int client_id, ResponseStatus st, const char *msg);
static void send_file_data(int socket_fd, const char *file_path, const char *filename);

static void send_response(Queue *resp_map, int client_id, ResponseStatus st, const char *msg) {
    printf("[send_response] Creating response for client_id=%d, status=%s\n", 
           client_id, st == RESP_OK ? "OK" : "ERR");
    fflush(stdout);
    
    Response *r = (Response *)calloc(1, sizeof(Response));
    if (!r) return;  // Out of memory
    
    r->client_id = client_id;
    r->status = st;
    strncpy(r->message, msg?msg:"", sizeof(r->message)-1);
    r->message[sizeof(r->message)-1] = '\0';  // Ensure null termination
    
    ResponseQueueEntry *entry = NULL;
    
    // Find the response queue for this client
    printf("[send_response] Searching for client_id=%d in response map (size=%zu)\n", 
           client_id, resp_map->size);
    fflush(stdout);
    
    pthread_mutex_lock(&resp_map->mutex);
    for (size_t i = 0; i < resp_map->size; i++) {
        ResponseQueueEntry *e = (ResponseQueueEntry *)resp_map->items[i].data;
        printf("[send_response] Checking entry %zu: client_id=%d\n", i, e->client_id);
        fflush(stdout);
        if (e->client_id == client_id) { 
            entry = e; 
            break; 
        }
    }
    pthread_mutex_unlock(&resp_map->mutex);
    
    if (entry) {
        printf("[send_response] Found entry for client_id=%d, pushing to queue\n", client_id);
        fflush(stdout);
        
        // Lock the entry's mutex before modifying its queue
        pthread_mutex_lock(&entry->mutex);
        
        // Add the response to the queue
        bool was_empty = (entry->queue.size == 0);
        queue_push(&entry->queue, r, PRIORITY_NORMAL);
        
        printf("[send_response] Response pushed, was_empty=%d, signaling...\n", was_empty);
        fflush(stdout);
        
        // Signal waiting threads if the queue was empty
        if (was_empty) {
            pthread_cond_signal(&entry->response_available);
        }
        
        pthread_mutex_unlock(&entry->mutex);
        printf("[send_response] Response delivered successfully\n");
        fflush(stdout);
    } else {
        printf("[send_response] ERROR: No entry found for client_id=%d!\n", client_id);
        fflush(stdout);
        free(r);  // No one is waiting for this response
    }
}

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

// Helper function to get the effective priority of a task
void *worker_thread_main(void *arg) {
    WorkerPoolArg *wpa = (WorkerPoolArg *)arg;
    
    while (1) {
        // Get the highest priority task
        Task *t = (Task *)queue_pop(wpa->task_queue);
        if (!t) {
            // No more tasks, exit thread
            break;
        }
        
        // Log task processing with thread ID for debugging
        printf("[Worker %lu] Processing task: %s (priority: %s, user: %s)\n",
               (unsigned long)pthread_self(),
               command_to_string(t->type),
               priority_to_string(t->priority),
               t->username[0] ? t->username : "anonymous");
        
        char err[256] = {0};
        bool task_completed = false;
		switch (t->type) {
			case CMD_SIGNUP: {
                bool ok = false;
                
                // Signup with the task's priority (default is PRIORITY_NORMAL)
                ok = user_store_signup(wpa->user_store, t->username, t->password, 100<<20, t->priority);
                
                if (!ok) {
                    snprintf(err, sizeof(err) - 1, "Signup failed - user may already exist");
                }
                
                send_response(wpa->resp_queues, t->client.client_id, 
                            ok ? RESP_OK : RESP_ERR, 
                            ok ? "signed_up" : (err[0] ? err : "signup_failed"));
                task_completed = true;
				break;
			}
			case CMD_LOGIN: {
                printf("[Worker] LOGIN: Starting login process for user '%s'\n", t->username);
                fflush(stdout);
                
                bool ok = false;
                char err[256] = {0};
                
                printf("[Worker] LOGIN: Calling user_store_lock_user...\n");
                fflush(stdout);
                User *u = user_store_lock_user(wpa->user_store, t->username);
                
                printf("[Worker] LOGIN: user_store_lock_user returned %p\n", (void*)u);
                fflush(stdout);
                
                if (u) {
                    printf("[Worker] LOGIN: Calling user_store_login...\n");
                    fflush(stdout);
                    ok = user_store_login(wpa->user_store, t->username, t->password);
                    printf("[Worker] LOGIN: user_store_login returned %d\n", ok);
                    fflush(stdout);
                    
                    if (!ok) {
                        snprintf(err, sizeof(err) - 1, "Invalid username or password");
                    }
                    
                    printf("[Worker] LOGIN: Calling user_store_unlock_user...\n");
                    fflush(stdout);
                    user_store_unlock_user(u);
                    printf("[Worker] LOGIN: user_store_unlock_user done\n");
                    fflush(stdout);
                } else {
                    snprintf(err, sizeof(err) - 1, "User not found");
                }
                
                printf("[Worker] Sending LOGIN response: %s (client_id=%d)\n", 
                       ok ? "OK" : "ERR", t->client.client_id);
                fflush(stdout);
                send_response(wpa->resp_queues, t->client.client_id, 
                            ok ? RESP_OK : RESP_ERR, 
                            ok ? "logged_in" : err);
                printf("[Worker] LOGIN response sent\n");
                fflush(stdout);
                task_completed = true;
                break;
            }
			case CMD_UPLOAD: {
                printf("[Worker] UPLOAD: Starting upload for user '%s', file '%s'\n", 
                       t->username, t->path);
                fflush(stdout);
                
                bool ok = false;
                char err[256] = {0};
                
                // Validate input parameters
                if (t->path[0] == '\0' || t->tmpfile[0] == '\0') {
                    snprintf(err, sizeof(err) - 1, "Invalid file path or temporary file");
                } else {
                    printf("[Worker] UPLOAD: Calling user_store_lock_user...\n");
                    fflush(stdout);
                    User *u = user_store_lock_user(wpa->user_store, t->username);
                    printf("[Worker] UPLOAD: user_store_lock_user returned %p\n", (void*)u);
                    fflush(stdout);
                    
                    if (u) {
                        // Check if user has sufficient priority for large uploads
                        if (t->size > (10 * 1024 * 1024) && u->priority < PRIORITY_HIGH) {
                            snprintf(err, sizeof(err) - 1, 
                                    "File too large (%.2f MB). Upgrade to high priority for larger uploads.", 
                                    (double)t->size / (1024 * 1024));
                        } else {
                            // Verify the temporary file exists and is accessible
                            FILE *f = fopen(t->tmpfile, "rb");
                            if (f) {
                                fclose(f);
                                ok = fs_upload(wpa->user_store, t->username, t->path, 
                                             t->tmpfile, t->size, err, sizeof(err) - 1);
                                // Clean up temp file after successful upload
                                if (ok) {
                                    unlink(t->tmpfile);
                                }
                            } else {
                                snprintf(err, sizeof(err) - 1, "Temporary file not found or inaccessible");
                            }
                        }
                        user_store_unlock_user(u);
                    } else {
                        snprintf(err, sizeof(err) - 1, "User not found or not logged in");
                    }
                }
                
                send_response(wpa->resp_queues, t->client.client_id, 
                            ok ? RESP_OK : RESP_ERR, 
                            ok ? "uploaded" : (err[0] ? err : "upload failed"));
                task_completed = true;
				break;
			}
			case CMD_DOWNLOAD: {
                char p[512] = {0};
                bool ok = false;
                char err[256] = {0};
                
                // Validate input parameters
                if (t->path[0] == '\0') {
                    snprintf(err, sizeof(err) - 1, "Invalid file path");
                    send_response(wpa->resp_queues, t->client.client_id, 
                                RESP_ERR, err);
                } else {
                    User *u = user_store_lock_user(wpa->user_store, t->username);
                    
                    if (u) {
                        ok = fs_download_path(wpa->user_store, t->username, t->path, 
                                            p, sizeof(p) - 1, err, sizeof(err) - 1);
                        
                        if (ok) {
                            if (u->priority == PRIORITY_HIGH) {
                                // For high-priority users, send file data immediately
                                send_response(wpa->resp_queues, t->client.client_id, 
                                            RESP_OK, "downloaded");
                                send_file_data(t->client.socket_fd, p, t->path);
                                // Clean up temp file
                                unlink(p);
                            } else {
                                // For normal/low priority users, send file path
                                send_response(wpa->resp_queues, t->client.client_id, 
                                            RESP_OK, p);
                            }
                        } else {
                            send_response(wpa->resp_queues, t->client.client_id, 
                                        RESP_ERR, err[0] ? err : "Download failed");
                        }
                        user_store_unlock_user(u);
                    } else {
                        snprintf(err, sizeof(err) - 1, "User not found or not logged in");
                        send_response(wpa->resp_queues, t->client.client_id, 
                                    RESP_ERR, err);
                    }
                }
                task_completed = true;
				break;
			}
			case CMD_DELETE: {
				bool ok = false;
                User *u = user_store_lock_user(wpa->user_store, t->username);
                if (u) {
                    ok = fs_delete(wpa->user_store, t->username, t->path, err, sizeof(err));
                    user_store_unlock_user(u);
                } else {
                    snprintf(err, sizeof(err), "User not found");
                }
                send_response(wpa->resp_queues, t->client.client_id, 
                            ok ? RESP_OK : RESP_ERR, 
                            ok ? "deleted" : err);
                task_completed = true;
				break;
			}
			case CMD_LIST: {
				char buf[4096] = {0};  // Increased buffer size for larger directory listings
                bool ok = false;
                User *u = user_store_lock_user(wpa->user_store, t->username);
                
                if (u) {
                    ok = fs_list(wpa->user_store, t->username, buf, sizeof(buf), err, sizeof(err));
                    user_store_unlock_user(u);
                } else {
                    snprintf(err, sizeof(err), "User not found");
                }
                
                send_response(wpa->resp_queues, t->client.client_id, 
                            ok ? RESP_OK : RESP_ERR, 
                            ok ? buf : err);
                task_completed = true;
                break;
            }
            default: break;
        }
        
        // Log task completion
        if (task_completed) {
            printf("[Worker %lu] Completed task: %s for user %s\n",
                   (unsigned long)pthread_self(),
                   command_to_string(t->type),
                   t->username[0] ? t->username : "anonymous");
        } else {
            printf("[Worker %lu] Failed to complete task: %s for user %s\n",
                   (unsigned long)pthread_self(),
                   command_to_string(t->type),
                   t->username[0] ? t->username : "anonymous");
        }
        
        // Free the task
        task_free(t);
        t = NULL;
    }
    
    printf("[Worker %lu] Shutting down\n", (unsigned long)pthread_self());
    return NULL;
}
