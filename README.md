# Operating Systems Lab 7 - File Server

## 0. Group Members
- M.Abdullah Iftikhar (BSCS-23070)
- Waqas Sohaib (BSCS-23173)
- Abdul Moiz (BSCS-23118)

## 1. Design Report

### System Architecture

#### 1.1 Thread Structure
- **Main Thread**
  - Initializes server components and thread pools
  - Listens for incoming client connections
  - Distributes client connections to the client thread pool
  - Handles graceful shutdown procedures

- **Client Thread Pool** (4 threads by default)
  - Each thread manages multiple client connections
  - Parses incoming commands into `Task` structures
  - Manages client state and authentication
  - Handles response delivery back to clients

- **Worker Thread Pool** (4 threads by default)
  - Processes tasks from the global task queue
  - Executes filesystem operations (read/write/delete)
  - Manages user authentication and file operations
  - Sends responses back through the response queue system

#### 1.2 Data Structures
- **Task Queue**
  - Thread-safe FIFO queue for pending operations
  - Shared between client and worker threads
  - Implements producer-consumer pattern with condition variables

- **Response Queues**
  - Per-client response queues for command results
  - Ensures ordered response delivery
  - Implements timeout and error handling

- **User Store**
  - Manages user authentication and file permissions
  - Implements thread-safe user session management
  - Handles user home directory structure

#### 1.3 File System Layer
- **Virtual File System**
  - Abstracts physical file operations
  - Implements user quota management
  - Handles concurrent access to shared resources
  - Provides atomic operations for file modifications

### Key Features

#### 2.1 Concurrency Control
- Fine-grained locking strategy
- Deadlock prevention mechanisms
- Thread-safe memory management
- Non-blocking I/O operations where applicable

#### 2.2 Error Handling
- Comprehensive error reporting
- Graceful degradation under load
- Resource cleanup on error conditions
- Client disconnection handling

#### 2.3 Security
- Secure password hashing
- File permission enforcement
- Input validation and sanitization
- Protection against path traversal attacks

#### 2.4 Performance
- Configurable thread pool sizes
- Efficient memory management
- Minimized critical sections
- Asynchronous I/O operations

## 2. Communication Mechanism

### Between Worker Thread Pool and Client Thread Pool
The system uses a **thread-safe queue** implementation for communication between components. The communication flow is as follows:

1. **Client Threads** (in `client_thread_main`):
   - Accept client connections and parse commands
   - Create `Task` objects for each client request
   - Push tasks to the global `task_queue` using `queue_push()`
   - Wait for responses on a per-client response queue

2. **Worker Threads** (in `worker_thread_main`):
   - Continuously wait for tasks using `queue_pop()` on the shared `task_queue`
   - Process different command types (SIGNUP, LOGIN, UPLOAD, DOWNLOAD, etc.)
   - Send responses back through the response queue system

3. **Queue Implementation** (in `queue.h`):
   - Uses `pthread_mutex_t` for thread safety
   - Implements `pthread_cond_t` for efficient waiting on empty queues
   - Supports thread-safe operations: `queue_push()`, `queue_pop()`, `queue_init()`, `queue_destroy()`
   - Handles proper cleanup on shutdown with `queue_close()`

4. **Response Handling**:
   - Each client has a dedicated response queue entry
   - Worker threads send responses using `send_response()`
   - Client threads block on their response queue until a response is available

This design provides:
- **Thread Safety**: Proper synchronization with mutexes and condition variables
- **Efficiency**: Non-blocking operations where possible
- **Scalability**: Configurable number of client and worker threads
- **Clean Shutdown**: Proper cleanup of resources on server shutdown

## 3. Build and Run Instructions

### Prerequisites
- POSIX-compliant system (Linux/macOS) or Windows with WSL/MSYS2
- GCC or Clang compiler
- GNU Make

### Building the Project
```bash
make
```

### Running the Server
```bash
./server <port>
```

### Running the Client
```bash
./client <host> <port>
```

## 4. Testing

### Memory Leaks
```bash
./testing/run_valgrind_simple.sh
```

### Race Conditions
```bash
./testing/run_tsan_simple.sh
```

### Test Reports
All test reports are saved in the `reports/` directory.

## 5. GitHub Repository
[GitHub Repository Link](https://github.com/yourusername/OS_LAB7)

**Important Note**: Any commits made after Thursday 11:59 PM will not be accepted. Please ensure all your work is committed and pushed before the deadline.

## 6. Protocol

The server supports the following commands:
- `SIGNUP <user> <pass>` - Create a new user account
- `LOGIN <user> <pass>` - Authenticate a user
- `UPLOAD <user> <relpath> <size> <tmp_src_path>` - Upload a file
- `DOWNLOAD <user> <relpath>` - Download a file
- `DELETE <user> <relpath>` - Delete a file
- `LIST <user>` - List user's files

For detailed client usage, see `CLIENT_README.md`.

