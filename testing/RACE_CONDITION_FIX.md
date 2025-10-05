# Race Condition Fix

## What ThreadSanitizer Found

When running `./run_tsan_simple.sh`, ThreadSanitizer detected a **data race** in the queue code:

```
WARNING: ThreadSanitizer: data race (pid=1620)
  Write of size 1 at 0x723c00000014 by main thread:
    #0 queue_close src/queue.c:14
    #1 on_sigint src/main.c:20
    
  Previous read of size 1 at 0x723c00000014 by thread T4:
    #0 queue_pop src/queue.c:53
    #1 worker_thread_main src/worker.c:32
```

## The Problem

The race condition occurred because:

1. **Signal Handler Issue**: The `on_sigint` signal handler (triggered by Ctrl+C) was calling `queue_close(&g_server->task_queue)` 
2. **Not Async-Signal-Safe**: `queue_close` uses `pthread_mutex_lock`, which is **NOT async-signal-safe**
3. **Race Detected**: While `queue_close` locks the mutex before setting `q->closed = true`, the signal handler can interrupt a thread that's already inside `queue_pop`, creating a potential race condition

## The Fix

### Changed Files

**1. `src/main.c` - Signal Handler**

**Before:**
```c
static void on_sigint(int s) { 
    (void)s; 
    if (g_server) {
        g_server->running = 0; 
        if (g_server->listen_fd >= 0) close(g_server->listen_fd); 
        queue_close(&g_server->task_queue);  // ❌ NOT async-signal-safe!
    }
}
```

**After:**
```c
static void on_sigint(int s) { 
    (void)s; 
    if (g_server) {
        g_server->running = 0;
        // Close listen socket to unblock accept()
        if (g_server->listen_fd >= 0) {
            shutdown(g_server->listen_fd, SHUT_RDWR);
            close(g_server->listen_fd);
        }
        // ✅ Don't call queue_close from signal handler!
    }
}
```

**2. `src/main.c` - Main Loop**

The `queue_close` calls are now done **after** the accept loop exits (not in the signal handler):

```c
while (server->running) {
    // ... accept connections ...
}

// ✅ Close queues here, not in signal handler
queue_close(&client_queue);
queue_close(&server->task_queue);
queue_close(&server->response_map);
```

**3. `include/server.h` - Proper Type for Signal Flag**

**Before:**
```c
volatile int running;
```

**After:**
```c
volatile sig_atomic_t running;  // ✅ Proper type for signal handlers
```

## Why This Fixes the Race

1. **Async-Signal-Safe**: The signal handler now only:
   - Sets `running = 0` (using `sig_atomic_t`, which is safe)
   - Calls `shutdown()` and `close()` (both async-signal-safe)

2. **Queue Operations in Main Thread**: `queue_close` is now called from the main thread after the accept loop exits, not from the signal handler

3. **Proper Synchronization**: All pthread operations happen in normal thread context, not from signal handlers

## Testing

To verify the fix works:

```bash
# Rebuild with ThreadSanitizer
make clean
make tsan

# Run the automated test
./run_tsan_simple.sh
```

**Expected Result:**
```
✓ SUCCESS: No race conditions detected!
```

## Key Takeaways

### ❌ Don't Do This in Signal Handlers:
- Call `pthread_mutex_lock/unlock`
- Call `pthread_cond_wait/signal/broadcast`
- Call any non-async-signal-safe functions

### ✅ Do This Instead:
- Set flags using `volatile sig_atomic_t`
- Use async-signal-safe functions only (see `man 7 signal-safety`)
- Perform cleanup in main thread after signal flag is set

## References

- [POSIX Async-Signal-Safe Functions](https://man7.org/linux/man-pages/man7/signal-safety.7.html)
- [ThreadSanitizer Documentation](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
- [Signal Handler Best Practices](https://www.gnu.org/software/libc/manual/html_node/Defining-Handlers.html)

---

**This is exactly why we use ThreadSanitizer!** It caught a subtle race condition that would be very hard to find through manual code review or testing alone.
