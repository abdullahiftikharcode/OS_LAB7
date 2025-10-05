# How to Check for Memory Leaks and Race Conditions

This guide shows you how to verify your multi-threaded file storage server has:
1. **No memory leaks** (using Valgrind)
2. **No race conditions** (using ThreadSanitizer)

## Prerequisites

```bash
# Install Valgrind (if not already installed)
sudo apt-get install valgrind

# GCC with ThreadSanitizer support (usually already available)
gcc --version  # Should be >= 4.8
```

## Step 1: Check for Memory Leaks

### Run Valgrind Test

```bash
# Make script executable (first time only)
chmod +x run_valgrind_simple.sh

# Run the script
./run_valgrind_simple.sh
```

The script will:
1. Build the server with debug symbols
2. Start the server under Valgrind
3. Wait for you to press Enter

### Test with Client

Open **another terminal** and run:

```bash
cd /mnt/c/Users/jivot/documents/OS_LAB7

# Run the debug client
./client_debug

# Perform some operations
> signup testuser testpass
> login testuser testpass  
> upload test.txt
> list
> download test.txt
> delete test.txt
> quit
```

### Stop Server and Check Results

Go back to the **first terminal** (where Valgrind is running) and press **Ctrl+C**.

Valgrind will show a summary. Look for:

**✓ PASS - No Memory Leaks:**
```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
   possibly lost: 0 bytes in 0 blocks
```

**✗ FAIL - Memory Leaks Detected:**
```
LEAK SUMMARY:
   definitely lost: 1,024 bytes in 8 blocks
   indirectly lost: 512 bytes in 4 blocks
```

The full report is saved to `valgrind_report.txt`.

---

## Step 2: Check for Race Conditions

### Run ThreadSanitizer Test

```bash
# Make script executable (first time only)
chmod +x run_tsan_simple.sh

# Run the script - it's fully automated!
./run_tsan_simple.sh
```

The script will **automatically**:
1. Build the server with ThreadSanitizer
2. Start the server with TSAN enabled
3. Run 4 different stress tests with concurrent operations (in batches to avoid overwhelming the server):
   - **Test 1:** 10 clients performing full operations (5 batches of 2)
   - **Test 2:** 20 rapid-fire operations on the same user (4 batches of 5)
   - **Test 3:** 15 clients doing mixed operations (5 batches of 3)
   - **Test 4:** 30 rapid connect/disconnect cycles (6 batches of 5)
4. Stop the server and analyze results

**You don't need to do anything** - just run the script and wait for results!

### Results

ThreadSanitizer will show results:

**✓ PASS - No Race Conditions:**
```
✓ No race conditions detected!
(No ThreadSanitizer reports generated)
```

**✗ FAIL - Race Conditions Detected:**
```
✗ Race conditions or warnings detected!

WARNING: ThreadSanitizer: data race (pid=1234)
  Write of size 4 at 0x7b0400001234 by thread T2:
    #0 worker_thread_main src/worker.c:45
```

Reports are saved to `tsan_report.txt*` files.

---

## Quick Reference

### Valgrind (Memory Leaks)
```bash
./run_valgrind_simple.sh
# In another terminal: ./client_debug → perform operations → quit
# Ctrl+C in server terminal
# Check: definitely lost = 0, indirectly lost = 0
```

### ThreadSanitizer (Race Conditions)
```bash
./run_tsan_simple.sh
# Fully automated - performs 4 stress tests automatically
# Check: No "WARNING: ThreadSanitizer" messages
```

---

## What the Code Changes Fixed

I've made the following improvements to prevent memory leaks:

1. **Added proper cleanup in `src/main.c`:**
   - Clean up remaining response queue entries before shutdown
   - Destroy all queues (client_queue, task_queue, response_map)
   - Free all allocated memory

2. **Updated Makefile:**
   - Added `debug` target for Valgrind (with `-g -O0`)
   - Added `tsan` target for ThreadSanitizer (with `-fsanitize=thread`)

3. **Created test scripts:**
   - `run_valgrind_simple.sh` - Interactive Valgrind testing
   - `run_tsan_simple.sh` - Interactive ThreadSanitizer testing

---

## Troubleshooting

### Valgrind shows "still reachable" memory
This is usually OK - it's global data that wasn't freed at program exit. Focus on "definitely lost" and "indirectly lost".

### ThreadSanitizer shows false positives
TSAN rarely has false positives. If you see warnings, investigate them carefully.

### Tests are slow
This is normal - Valgrind can slow execution 10-50x, and TSAN also adds overhead.

### Can't find netcat
Install it: `sudo apt-get install netcat`

---

## Files Created

- `valgrind_report.txt` - Detailed Valgrind output
- `tsan_report.txt*` - ThreadSanitizer reports (if issues found)
- `server_debug` - Debug build for Valgrind
- `server_tsan` - TSAN build for race detection
- `client_debug` - Debug build of client

Clean up with: `make clean && rm -f *.txt tsan_report.txt*`

---

## Summary

✅ **To pass both checks, you need:**
1. Valgrind: 0 bytes definitely lost, 0 bytes indirectly lost
2. ThreadSanitizer: No warnings

Both tools are industry-standard for finding concurrency bugs and memory issues in C programs!
