# Multi-threaded File Server Design Report

## 1. System Architecture

### 1.1 Thread Pools
- **Client Thread Pool**
  - Handles incoming client connections
  - Reads commands and creates tasks
  - Manages client response queues

- **Worker Thread Pool**
  - Processes tasks from the task queue
  - Executes file operations
  - Sends responses back to clients

### 1.2 Thread-Safe Queues

#### Client Queue
- Implemented as a thread-safe FIFO queue
- Stores incoming client connections
- Protected by a mutex and condition variable
- Used by client threads to accept new connections

#### Task Queue
- Implemented as a priority queue (max-heap)
- Tasks are ordered by priority and enqueue time
- Thread-safe operations with proper synchronization
- Used by worker threads to get the next task

## 2. Synchronization

### 2.1 Mutexes
- `queue_mutex`: Protects access to the task queue
- `response_mutex`: Protects client response queues
- `user_mutex`: Protects user data structures
- `file_mutex`: Protects file system operations

### 2.2 Condition Variables
- `queue_cond`: Signals when tasks are available
- `response_cond`: Signals when responses are ready
- `shutdown_cond`: Signals server shutdown

### 2.3 Deadlock Prevention
- Consistent lock ordering (queue → response → user → file)
- Timeouts on condition variable waits
- Non-blocking operations where possible

## 3. Worker-to-Client Communication

### 3.1 Response Queues
- Each client has a dedicated response queue
- Worker threads enqueue responses
- Client threads dequeue responses
- Uses condition variables for efficient waiting

### 3.2 Response Format
```
<STATUS> <MESSAGE>\n
- STATUS: OK or ERR
- MESSAGE: Response data or error message
```

## 4. File Operations

### 4.1 Upload
1. Read file in chunks
2. Encode data using XOR cipher
3. Write to user's directory
4. Update user's storage usage

### 4.2 Download
1. Read encoded file in chunks
2. Decode data using XOR cipher
3. Send to client
4. Clean up temporary files

### 4.3 Concurrency Control
- User-level locking for operations
- Atomic file operations
- Proper error handling and cleanup

## 5. Error Handling
- All system calls are checked for errors
- Resources are properly cleaned up
- Meaningful error messages are returned to clients
- Timeouts prevent indefinite blocking

## 6. Security Considerations
- Input validation on all commands
- Safe file path handling
- Proper file permissions
- Secure temporary file handling

## 7. Performance
- Efficient memory usage with chunked I/O
- Non-blocking operations where possible
- Thread pools prevent resource exhaustion
- Proper queue management prevents unbounded growth

## 8. Future Improvements
1. Add user authentication tokens
2. Implement file compression
3. Add support for partial file transfers
4. Improve error recovery
5. Add more detailed logging
