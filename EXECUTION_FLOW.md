# Multi-threaded File Storage Server - Complete Execution Flow

This document explains the complete execution flow of the multi-threaded file storage server, from startup to shutdown, showing how data flows through the system and how threads interact.

## System Architecture Overview

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Main Thread   │    │  Client Threads  │    │ Worker Threads  │
│                 │    │                  │    │                 │
│  - Accepts      │───▶│  - Parse cmds    │───▶│  - Execute ops  │
│    connections  │    │  - Enqueue tasks │    │  - Send responses│
│  - Routes to    │    │  - Wait for resp │    │                 │
│    client pool  │    │  - Write to sock │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  Client Queue   │    │   Task Queue     │    │ Response Queues │
│                 │    │                  │    │                 │
│ ClientInfo*     │    │ Task*            │    │ Response*       │
│ (per connection)│    │ (per command)    │    │ (per client)    │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

## 1. Server Startup Flow

### 1.1 Initialization Phase
```c
// main.c lines 814-838
int main(int argc, char **argv) {
    unsigned short port = 9090;
    int client_threads = 4;
    int worker_threads = 4;
    if (argc > 1) port = (unsigned short)atoi(argv[1]);
    
    // Create server state
    ServerState *server = (ServerState *)calloc(1, sizeof(ServerState));
    if (!server) {
        fprintf(stderr, "Failed to allocate server state\n");
        return 1;
    }
    
    server->user_store = user_store_create("storage");
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
```

**What happens:**
1. Parse command line arguments (port number)
2. **Create ServerState**: Allocate and initialize server state structure
3. **Create UserStore**: Initialize user storage system with struct-based design
4. Initialize global task queue (where client threads put work)
5. Initialize global response map (maps client_id → ResponseQueue)
6. Set up signal handler for graceful shutdown
7. Store server state globally for signal handler access

### 1.2 Network Setup
```c
// main.c lines 20-28
static int tcp_listen(unsigned short port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt=1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET; 
    addr.sin_port = htons(port); 
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(fd, 64);
    return fd;
}
```

**What happens:**
1. Create TCP socket
2. Set SO_REUSEADDR to allow immediate reuse of port
3. Bind to all interfaces on specified port
4. Start listening with backlog of 64 connections

### 1.3 Thread Pool Creation
```c
// main.c lines 844-852
// launch worker threads
pthread_t *wts = calloc((size_t)worker_threads, sizeof(pthread_t));
WorkerPoolArg wpa = { .task_queue=&server->task_queue, .resp_queues=&server->response_map, .user_store=server->user_store };
for (int i=0;i<worker_threads;i++) pthread_create(&wts[i], NULL, worker_thread_main, &wpa);

Queue client_queue; queue_init(&client_queue);
pthread_t *cts = calloc((size_t)client_threads, sizeof(pthread_t));
ClientThreadArg cta = { .client_queue=&client_queue };
for (int i=0;i<client_threads;i++) pthread_create(&cts[i], NULL, client_thread_main, &cta);
```

**What happens:**
1. Create worker thread pool (default 4 threads)
2. Each worker gets access to server's task queue, response map, and **UserStore instance**
3. Create client thread pool (default 4 threads)
4. Each client thread gets access to client queue
5. All threads start running their respective main functions

## 2. Connection Handling Flow

### 2.1 Accept Loop
```c
// main.c lines 84-97
int next_client_id = 1;
while (g_running) {
    struct sockaddr_in cli; socklen_t cl = sizeof(cli);
    int cfd = accept(lfd, (struct sockaddr*)&cli, &cl);
    if (cfd < 0) {
        if (!g_running) break; else continue;
    }
    cta.client_id = next_client_id;
    register_client_response_queue(next_client_id);
    ClientInfo *ci = (ClientInfo *)calloc(1, sizeof(ClientInfo));
    ci->socket_fd = cfd; ci->client_id = next_client_id;
    next_client_id++;
    queue_push(&client_queue, ci);
}
```

**What happens for each connection:**
1. Block on `accept()` until client connects
2. Generate unique client_id
3. Create ResponseQueueEntry for this client
4. Allocate ClientInfo with socket_fd and client_id
5. Push ClientInfo to client queue (any client thread can pick it up)

### 2.2 Response Queue Registration
```c
// main.c lines 784-790
static ResponseQueueEntry *register_client_response_queue(ServerState *server, int client_id) {
    ResponseQueueEntry *e = (ResponseQueueEntry *)calloc(1, sizeof(ResponseQueueEntry));
    e->client_id = client_id;
    queue_init(&e->queue);
    queue_push(&server->response_map, e);
    return e;
}
```

**What happens:**
1. Allocate new ResponseQueueEntry
2. Initialize its internal queue
3. Add to server's response map (implemented as a queue of entries)

## 3. Client Thread Processing Flow

### 3.1 Client Thread Main Loop
```c
// client.c lines 16-84
void *client_thread_main(void *arg) {
    ClientThreadArg *cta = (ClientThreadArg *)arg;
    char line[512];
    while (1) {
        ClientInfo *ci = (ClientInfo *)queue_pop(cta->client_queue);
        if (!ci) break;  // Queue closed, exit thread
        int fd = ci->socket_fd;
        int client_id = ci->client_id;
        free(ci);
        // Process this connection...
    }
    return NULL;
}
```

**What happens:**
1. Block on client queue waiting for new connections
2. When ClientInfo arrives, extract socket_fd and client_id
3. Process all commands from this connection
4. When connection ends, go back to waiting for next connection

### 3.2 Command Processing Loop
```c
// client.c lines 25-77
while (1) {
    int r = recv_line(fd, line, sizeof(line));
    if (r <= 0) break;  // Connection closed
    
    Task *task = (Task *)calloc(1, sizeof(Task));
    task->client.client_id = client_id;
    task->client.socket_fd = fd;
    
    // Parse command and fill task...
    if (strncmp(line, "SIGNUP", 6) == 0) {
        task->type = CMD_SIGNUP;
        sscanf(line+6, "%63s %63s", task->username, task->password);
    } else if (strncmp(line, "LOGIN", 5) == 0) {
        // ... similar for other commands
    }
    
    // Send task to worker pool
    extern Queue g_task_queue;
    queue_push(&g_task_queue, task);
    
    // Wait for response...
}
```

**What happens for each command:**
1. Read line from socket
2. Allocate new Task
3. Parse command and fill task fields
4. Push task to global task queue
5. Wait for response from worker

### 3.3 Response Handling
```c
// client.c lines 63-76
extern Queue g_response_map;
ResponseQueueEntry *entry = NULL;
QueueNode *scan;
pthread_mutex_lock(&g_server->response_map.mutex);
scan = g_server->response_map.head;
while (scan) {
    ResponseQueueEntry *e = (ResponseQueueEntry *)scan->data;
    if (e->client_id == client_id) { entry = e; break; }
    scan = scan->next;
}
pthread_mutex_unlock(&g_server->response_map.mutex);

if (!entry) break;
Response *resp = (Response *)queue_pop(&entry->queue);
if (!resp) break;
char out[1024];
snprintf(out, sizeof(out), "%s %s\n", resp->status==RESP_OK?"OK":"ERR", resp->message);
write(fd, out, strlen(out));
free(resp);
```

**What happens:**
1. Find this client's response queue in the server's response map
2. Block waiting for response
3. When response arrives, format it as "OK/ERR message"
4. Write response back to socket
5. Free response memory

## 4. Worker Thread Processing Flow

### 4.1 Worker Thread Main Loop
```c
// worker.c lines 26-77
void *worker_thread_main(void *arg) {
    WorkerPoolArg *wpa = (WorkerPoolArg *)arg;
    while (1) {
        Task *t = (Task *)queue_pop(wpa->task_queue);
        if (!t) break;  // Queue closed, exit thread
        
        char err[256] = {0};
        switch (t->type) {
            case CMD_SIGNUP: {
                bool ok = user_store_signup(wpa->user_store, t->username, t->password, 100<<20);
                send_response(wpa->resp_queues, t->client.client_id, 
                             ok?RESP_OK:RESP_ERR, ok?"signed_up":"signup_failed");
                break;
            }
            case CMD_LOGIN: {
                bool ok = user_store_login(wpa->user_store, t->username, t->password);
                send_response(wpa->resp_queues, t->client.client_id, 
                             ok?RESP_OK:RESP_ERR, ok?"logged_in":"login_failed");
                break;
            }
            // ... handle other commands with UserStore
        }
        free(t);
    }
    return NULL;
}
```

**What happens:**
1. Block on task queue waiting for work
2. When task arrives, process based on command type
3. **Execute operations using UserStore instance** (not global functions)
4. Send response back to client
5. Free task memory
6. Repeat

### 4.2 Response Sending
```c
// worker.c lines 8-24
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
```

**What happens:**
1. Allocate new Response
2. Fill response with status and message
3. Find client's response queue in the map
4. Push response to that queue (wakes up waiting client thread)
5. If client not found (disconnected), free response

## 5. File Operations Flow

### 5.1 Upload Operation
```c
// worker.c lines 43-51
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
```

**What happens:**
1. Acquire per-user lock from **UserStore instance**
2. Call filesystem upload function with **UserStore parameter**
3. Release per-user lock
4. Send response

### 5.2 Filesystem Upload Implementation
```c
// fs.c lines 12-29
bool fs_upload(UserStore *store, const char *user, const char *dst_relpath, const char *tmp_src, size_t size, char *err, size_t errlen) {
    char dst[512];
    build_user_path(store, user, dst_relpath, dst, sizeof(dst));
    FILE *in = fopen(tmp_src, "rb");
    if (!in) { snprintf(err, errlen, "open tmp_src failed"); return false; }
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); snprintf(err, errlen, "open dst failed"); return false; }
    char buf[8192];
    size_t total = 0, n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
        total += n;
    }
    fclose(in);
    fclose(out);
    return true;
}
```

**What happens:**
1. Build destination path using **UserStore's storage root**
2. Open source file (server-side temp file)
3. Open destination file
4. Copy file contents in 8KB chunks
5. Close both files

## 6. Synchronization and Locking Flow

### 6.1 Per-User Locking
```c
// user.c lines 98-113
User* user_store_lock_user(UserStore *store, const char *name) {
    pthread_mutex_lock(&store->users_mutex);
    
    UserNode *n = store->head;
    while (n) {
        if (strcmp(n->user.name, name) == 0) {
            pthread_mutex_lock(&n->user.mutex);
            pthread_mutex_unlock(&store->users_mutex);
            return &n->user;
        }
        n = n->next;
    }
    
    pthread_mutex_unlock(&store->users_mutex);
    return NULL;
}

void user_store_unlock_user(User *user) {
    if (user) pthread_mutex_unlock(&user->mutex);
}
```

**What happens:**
1. Lock **UserStore's** users table mutex
2. Find user by name in the store's linked list
3. If found, acquire user's per-user mutex
4. Release UserStore's users table mutex
5. Return user pointer (or NULL if not found)

**Why this works:**
- **UserStore's users_mutex** protects the user table during lookup
- Per-user mutex serializes operations on that user's files
- Multiple users can operate concurrently
- Same user's operations are serialized
- **Each UserStore instance has its own mutex** (no global state)

### 6.2 Queue Synchronization
```c
// queue.c lines 51-68
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
```

**What happens:**
1. Acquire queue mutex
2. If queue is empty and not closed, wait on condition variable
3. When woken up, check if queue is still empty (might be closed)
4. Remove head node
5. Update head/tail pointers
6. Release mutex and return data

## 7. Complete Request Flow Example

Let's trace a complete SIGNUP request:

### 7.1 Client Connection
1. **Main thread**: `accept()` returns socket_fd=5, assigns client_id=1
2. **Main thread**: Creates ResponseQueueEntry for client_id=1
3. **Main thread**: Creates ClientInfo{fd=5, client_id=1}
4. **Main thread**: Pushes ClientInfo to client_queue

### 7.2 Command Processing
1. **Client thread**: Pops ClientInfo from client_queue
2. **Client thread**: Reads "SIGNUP alice password123" from socket
3. **Client thread**: Creates Task{type=CMD_SIGNUP, username="alice", password="password123", client_id=1}
4. **Client thread**: Pushes Task to g_task_queue
5. **Client thread**: Waits on client_id=1's response queue

### 7.3 Task Execution
1. **Worker thread**: Pops Task from g_task_queue
2. **Worker thread**: Calls `user_signup("alice", "password123", 100MB)`
3. **Worker thread**: Inside user_signup:
   - Locks users_mutex
   - Checks if "alice" already exists
   - Creates new UserNode with per-user mutex
   - Creates directory `storage/alice/`
   - Unlocks users_mutex
4. **Worker thread**: Calls `send_response()` with success status

### 7.4 Response Delivery
1. **Worker thread**: Finds ResponseQueueEntry for client_id=1
2. **Worker thread**: Creates Response{client_id=1, status=RESP_OK, message="signed_up"}
3. **Worker thread**: Pushes Response to client_id=1's response queue
4. **Client thread**: Wakes up from `queue_pop()` on response queue
5. **Client thread**: Formats response as "OK signed_up"
6. **Client thread**: Writes response to socket
7. **Client thread**: Frees Response memory

## 8. Shutdown Flow

### 8.1 Signal Handling
```c
// main.c lines 23-30
static void on_sigint(int s) { 
    (void)s; 
    if (g_server) {
        g_server->running = 0; 
        if (g_server->listen_fd >= 0) close(g_server->listen_fd); 
        queue_close(&g_server->task_queue); 
    }
}
```

**What happens on Ctrl+C:**
1. Set server->running = 0
2. Close listen socket (causes accept() to fail)
3. Close task queue (wakes up all worker threads)

### 8.2 Graceful Shutdown
```c
// main.c lines 127-137
queue_close(&client_queue);
queue_close(&server->task_queue);
queue_close(&server->response_map);
for (int i=0;i<client_threads;i++) pthread_join(cts[i], NULL);
free(cts);
for (int i=0;i<worker_threads;i++) pthread_join(wts[i], NULL);
free(wts);
user_store_destroy(server->user_store);
free(server);
return 0;
```

**What happens:**
1. Close all queues (wakes up waiting threads)
2. Wait for all client threads to exit
3. Wait for all worker threads to exit
4. Free thread arrays
5. **Destroy UserStore** (cleanup all users and mutexes)
6. Free server state
7. Exit

## 9. Data Flow Summary

```
Client Socket → Client Thread → Task Queue → Worker Thread → Response Queue → Client Thread → Client Socket
     ↑                                                                                              ↓
     └─────────────────────────────────── Response Map ←────────────────────────────────────────────┘
```

**Key Points:**
- **Producer/Consumer**: Client threads produce tasks, worker threads consume them
- **Response Routing**: Each client has its own response queue for proper message delivery
- **Synchronization**: Queues use mutexes + condition variables for thread-safe operations
- **Per-User Locking**: Prevents race conditions on same user's files
- **Graceful Shutdown**: Signal handling ensures clean thread termination
- **Struct-Based Design**: UserStore encapsulates user state, ServerState encapsulates server state
- **No Global Variables**: All state is properly encapsulated in structs

This architecture ensures:
- **Scalability**: Multiple clients and workers can operate concurrently
- **Safety**: Proper locking prevents data races
- **Responsiveness**: Non-blocking accept loop with thread pools
- **Reliability**: Graceful shutdown and proper resource cleanup
- **Maintainability**: Clean separation of concerns with struct-based design
- **Testability**: Each component can be tested independently
- **Extensibility**: Easy to add new features or modify existing ones
