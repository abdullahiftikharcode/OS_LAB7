# Testing Guide: Memory Leaks and Race Conditions

This guide explains how to check your multi-threaded file storage server for memory leaks and race conditions.

## Prerequisites

### For Valgrind (Memory Leak Detection)
```bash
# Ubuntu/Debian
sudo apt-get install valgrind

# macOS (requires Homebrew)
brew install valgrind

# Fedora/RHEL
sudo dnf install valgrind
```

### For ThreadSanitizer (Race Condition Detection)
- GCC >= 4.8 or Clang >= 3.2
- Usually already available on modern Linux systems

### Additional Tools
```bash
# Install netcat for testing (if not already installed)
# Ubuntu/Debian
sudo apt-get install netcat

# macOS (usually pre-installed)
# If needed: brew install netcat
```

## Quick Start

### 1. Test for Memory Leaks with Valgrind

```bash
# Make script executable
chmod +x run_valgrind_simple.sh

# Run the test
./run_valgrind_simple.sh

# In another terminal, run client and perform operations:
./client_debug
> signup testuser testpass
> login testuser testpass
> upload myfile.txt
> list
> quit

# Back in server terminal, press Ctrl+C
```

**Expected Output:**
```
Leak Summary:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
```

### 2. Test for Race Conditions with ThreadSanitizer

```bash
# Make script executable
chmod +x run_tsan_simple.sh

# Run the test
./run_tsan_simple.sh

# In multiple other terminals, run clients concurrently:
# Terminal 2: ./client
# Terminal 3: ./client
# Terminal 4: ./client
# Perform operations in all clients simultaneously

# Back in server terminal, press Ctrl+C
```

**Expected Output:**
```
✓ No race conditions detected!
```

## Manual Testing

### Manual Valgrind Testing

1. **Build debug version:**
   ```bash
   make debug
   ```

2. **Run server with Valgrind:**
   ```bash
   valgrind --leak-check=full \
            --show-leak-kinds=all \
            --track-origins=yes \
            --verbose \
            --log-file=valgrind.log \
            ./server_debug 9090
   ```

3. **In another terminal, run your client:**
   ```bash
   ./client localhost 9090
   ```

4. **Perform operations:**
   ```
   > signup testuser testpass
   > login testuser testpass
   > upload myfile.txt
   > list
   > download myfile.txt
   > delete myfile.txt
   > quit
   ```

5. **Stop the server:**
   - Press `Ctrl+C` in the server terminal

6. **Check results:**
   ```bash
   cat valgrind.log | grep "LEAK SUMMARY" -A 5
   ```

   **Good output:**
   ```
   LEAK SUMMARY:
      definitely lost: 0 bytes in 0 blocks
      indirectly lost: 0 bytes in 0 blocks
      possibly lost: 0 bytes in 0 blocks
   ```

### Manual ThreadSanitizer Testing

1. **Build with ThreadSanitizer:**
   ```bash
   make tsan
   ```

2. **Set TSAN options:**
   ```bash
   export TSAN_OPTIONS="log_path=tsan.log second_deadlock_stack=1"
   ```

3. **Run server:**
   ```bash
   ./server_tsan 9090
   ```

4. **Run multiple clients concurrently:**
   ```bash
   # Terminal 2
   ./client localhost 9090
   
   # Terminal 3
   ./client localhost 9090
   
   # Terminal 4
   ./client localhost 9090
   ```

5. **Perform concurrent operations in all clients**

6. **Stop server and check logs:**
   ```bash
   cat tsan.log*
   ```

   **Good output:** No warnings or empty files
   
   **Bad output:** Contains "WARNING: ThreadSanitizer: data race"

## Understanding the Results

### Valgrind Output

#### ✓ Good (No Leaks)
```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
   possibly lost: 0 bytes in 0 blocks
   still reachable: 0 bytes in 0 blocks

ERROR SUMMARY: 0 errors from 0 contexts
```

#### ✗ Bad (Memory Leaks)
```
LEAK SUMMARY:
   definitely lost: 1,024 bytes in 8 blocks
   indirectly lost: 512 bytes in 4 blocks
```

**What to look for:**
- **definitely lost**: Direct memory leaks - must fix!
- **indirectly lost**: Memory lost through pointers - must fix!
- **possibly lost**: May be false positives, but investigate
- **still reachable**: Memory not freed at exit (often acceptable for global data)

### ThreadSanitizer Output

#### ✓ Good (No Race Conditions)
- No output files or empty files
- No "WARNING" messages

#### ✗ Bad (Race Condition Detected)
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x7b0400001234 by thread T2:
    #0 worker_thread_main src/worker.c:45
    
  Previous read of size 4 at 0x7b0400001234 by thread T1:
    #0 client_thread_main src/client.c:67
```

**What to look for:**
- **data race**: Multiple threads accessing same memory without proper synchronization
- **lock-order-inversion**: Potential deadlock situation
- File and line numbers where the race occurs

## Common Issues and Fixes

### Memory Leaks

1. **Forgetting to free allocated memory:**
   ```c
   // Bad
   Task *task = malloc(sizeof(Task));
   // ... use task ...
   // Missing: free(task);
   
   // Good
   Task *task = malloc(sizeof(Task));
   // ... use task ...
   free(task);
   ```

2. **Not destroying mutexes/condition variables:**
   ```c
   // Bad
   pthread_mutex_init(&mutex, NULL);
   // Missing: pthread_mutex_destroy(&mutex);
   
   // Good
   pthread_mutex_init(&mutex, NULL);
   // ... use mutex ...
   pthread_mutex_destroy(&mutex);
   ```

3. **Not closing file descriptors:**
   ```c
   // Bad
   int fd = socket(...);
   // Missing: close(fd);
   
   // Good
   int fd = socket(...);
   // ... use fd ...
   close(fd);
   ```

### Race Conditions

1. **Accessing shared data without locks:**
   ```c
   // Bad
   global_counter++;  // Multiple threads can access
   
   // Good
   pthread_mutex_lock(&mutex);
   global_counter++;
   pthread_mutex_unlock(&mutex);
   ```

2. **Inconsistent lock ordering (deadlock risk):**
   ```c
   // Bad - Thread 1
   pthread_mutex_lock(&lock_a);
   pthread_mutex_lock(&lock_b);
   
   // Bad - Thread 2
   pthread_mutex_lock(&lock_b);  // Different order!
   pthread_mutex_lock(&lock_a);
   
   // Good - Always same order
   pthread_mutex_lock(&lock_a);
   pthread_mutex_lock(&lock_b);
   ```

3. **Reading/writing shared variables without synchronization:**
   ```c
   // Bad
   if (queue->closed) { ... }  // No lock!
   
   // Good
   pthread_mutex_lock(&queue->mutex);
   if (queue->closed) { ... }
   pthread_mutex_unlock(&queue->mutex);
   ```

## Continuous Integration

You can add these tests to your CI pipeline:

```bash
# In your CI script
make clean
./test_valgrind.sh || exit 1
./test_tsan.sh || exit 1
echo "All tests passed!"
```

## Troubleshooting

### Valgrind Issues

**Problem:** Valgrind shows false positives from system libraries
**Solution:** Use suppression files:
```bash
valgrind --suppressions=/usr/share/glib-2.0/valgrind/glib.supp ...
```

**Problem:** Valgrind is very slow
**Solution:** This is normal - Valgrind can slow execution 10-50x

### ThreadSanitizer Issues

**Problem:** TSAN reports false positives
**Solution:** Review the code carefully - TSAN rarely has false positives

**Problem:** Program crashes with TSAN
**Solution:** Reduce optimization level (already set to -O1 in Makefile)

**Problem:** TSAN not available
**Solution:** Use newer GCC/Clang or try Helgrind (Valgrind's race detector):
```bash
valgrind --tool=helgrind ./server_debug 9090
```

## Best Practices

1. **Test regularly** - Run these tests after every significant change
2. **Test with realistic workloads** - Use multiple concurrent clients
3. **Test edge cases** - Client disconnections, rapid requests, large files
4. **Fix issues immediately** - Don't accumulate technical debt
5. **Document suppressions** - If you must suppress warnings, document why

## Additional Resources

- [Valgrind Manual](https://valgrind.org/docs/manual/manual.html)
- [ThreadSanitizer Documentation](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
- [POSIX Threads Programming](https://hpc-tutorials.llnl.gov/posix/)

## Makefile Targets Reference

```bash
make              # Build optimized version
make debug        # Build debug version for Valgrind
make tsan         # Build with ThreadSanitizer
make clean        # Remove all binaries
make valgrind     # Shows Valgrind command
```
