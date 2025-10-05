# Quick Testing Reference

## TL;DR - Run These Commands

```bash
# 1. Test for memory leaks (semi-automated)
./run_valgrind_simple.sh
# Then in another terminal: ./client_debug
# Perform operations, quit client, Ctrl+C server

# 2. Test for race conditions (fully automated)
./run_tsan_simple.sh
# Just wait - it runs 4 stress tests automatically!
```

## What Each Test Does

### `run_valgrind_simple.sh` - Memory Leak Detection
- Builds server with debug symbols (`-g -O0`)
- Runs server under Valgrind with full leak checking
- You manually run client and perform operations
- After stopping server, shows leak summary
- **Pass criteria:** 0 bytes definitely lost, 0 bytes indirectly lost

### `run_tsan_simple.sh` - Race Condition Detection (Automated)
- Builds server with ThreadSanitizer instrumentation (`-fsanitize=thread`)
- Runs server with TSAN enabled
- **Automatically** performs 4 stress tests in batches:
  - 10 clients with full operations (5 batches of 2)
  - 20 rapid-fire same-user operations (4 batches of 5)
  - 15 mixed concurrent operations (5 batches of 3)
  - 30 rapid connect/disconnect cycles (6 batches of 5)
- Stops server and shows results automatically
- **Pass criteria:** No ThreadSanitizer warnings
- **Duration:** ~30-60 seconds

## Expected Output

### ✓ Success
```
========================================
Valgrind Results
========================================

✓ SUCCESS: No memory leaks detected!
✓ No memory errors detected

========================================
Testing Complete!
========================================
```

```
========================================
ThreadSanitizer Results
========================================

✓ SUCCESS: No race conditions detected!

========================================
Testing Complete!
========================================
```

### ✗ Failure

If tests fail, check the log files:
- `valgrind_server.log` - Memory leak details
- `tsan_report.txt*` - Race condition details

## Manual Testing (If Scripts Don't Work)

### Valgrind
```bash
make debug
valgrind --leak-check=full --show-leak-kinds=all ./server_debug 9090
# In another terminal: ./client
# Ctrl+C to stop server
# Check output for "definitely lost: 0 bytes"
```

### ThreadSanitizer
```bash
make tsan
export TSAN_OPTIONS="log_path=tsan.log"
./server_tsan 9090
# In other terminals: run multiple ./client instances
# Ctrl+C to stop
# Check: cat tsan.log* (should be empty or no warnings)
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `valgrind: command not found` | Install: `sudo apt-get install valgrind` |
| `nc: command not found` | Install: `sudo apt-get install netcat` |
| TSAN not supported | Use GCC >= 4.8 or Clang >= 3.2 |
| Scripts fail on macOS | Use Linux or WSL (better POSIX support) |
| False positives | Check TESTING_GUIDE.md for details |

## Quick Fixes for Common Issues

### Memory Leak
```c
// Always pair malloc with free
void *ptr = malloc(size);
// ... use ptr ...
free(ptr);

// Always destroy mutexes
pthread_mutex_destroy(&mutex);
pthread_cond_destroy(&cond);

// Always close file descriptors
close(fd);
```

### Race Condition
```c
// Always protect shared data with locks
pthread_mutex_lock(&mutex);
shared_variable++;
pthread_mutex_unlock(&mutex);

// Use consistent lock ordering
// Always lock A before B, never B before A
```

## Files Created by Tests

- `valgrind_server.log` - Valgrind detailed report
- `tsan_report.txt*` - ThreadSanitizer reports
- `server_debug` - Debug build binary
- `server_tsan` - TSAN build binary
- `test_upload.txt` - Temporary test file

Clean up with: `make clean && rm -f *.log tsan_report.txt* test_upload.txt`
