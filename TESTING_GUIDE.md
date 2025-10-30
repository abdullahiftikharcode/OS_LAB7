# Testing Guide for OS Lab 7 - Multi-threaded File Server

## Prerequisites

Before testing, ensure you have:
- GCC compiler with pthread support
- Make utility
- Valgrind (for memory leak testing)
- ThreadSanitizer support in GCC (for race condition testing)
- SQLite3 library

## Quick Start Testing

### 1. Basic Compilation Test

```bash
# Clean and build
make clean && make

# Expected output: No errors, generates 'server' and 'client' binaries
```

If you see compilation errors, they should all be fixed now. The build should complete successfully.

### 2. Basic Functionality Test

**Terminal 1 - Start Server:**
```bash
./server 9090
```

Expected output:
```
Server listening on 9090
```

**Terminal 2 - Run Client:**
```bash
./client 127.0.0.1 9090
```

Then test these commands in the client:
```
SIGNUP testuser testpass
LOGIN testuser testpass
UPLOAD testuser test.txt <size> /path/to/test.txt
LIST testuser
DOWNLOAD testuser test.txt
DELETE testuser test.txt
QUIT
```

## Detailed Testing Procedures

### Test 1: Memory Leak Testing (Valgrind)

**Purpose**: Verify no memory leaks exist in the server

**Steps**:

1. Build debug version:
```bash
make clean && make debug
```

2. Run Valgrind test script:
```bash
./testing/run_valgrind_simple.sh
```

3. When prompted, press Enter to start the server

4. In another terminal, run the client:
```bash
./client_debug 127.0.0.1 9090
```

5. Perform several operations:
```
SIGNUP user1 pass1
LOGIN user1 pass1
UPLOAD user1 file1.txt 100 /tmp/test.txt
LIST user1
DOWNLOAD user1 file1.txt
DELETE user1 file1.txt
QUIT
```

6. Stop the server with Ctrl+C

7. Check the report:
```bash
cat reports/valgrind_report.txt
```

**Expected Result**:
```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
   possibly lost: 0 bytes in 0 blocks

ERROR SUMMARY: 0 errors from 0 contexts
```

**Note**: Some "still reachable" blocks from SQLite or pthread are acceptable.

---

### Test 2: Race Condition Testing (ThreadSanitizer)

**Purpose**: Verify no race conditions exist

**Steps**:

1. Build with ThreadSanitizer:
```bash
make clean && make tsan
```

2. Run TSAN test script:
```bash
./testing/run_tsan_simple.sh
```

3. When prompted, press Enter to start the server

4. In another terminal, run multiple clients simultaneously:

**Terminal 2:**
```bash
./client 127.0.0.1 9090
```
Commands:
```
SIGNUP user1 pass1
LOGIN user1 pass1
UPLOAD user1 file1.txt 100 /tmp/test1.txt
LIST user1
QUIT
```

**Terminal 3:**
```bash
./client 127.0.0.1 9090
```
Commands:
```
SIGNUP user2 pass2
LOGIN user2 pass2
UPLOAD user2 file2.txt 100 /tmp/test2.txt
LIST user2
QUIT
```

**Terminal 4:**
```bash
./client 127.0.0.1 9090
```
Commands:
```
SIGNUP user3 pass3
LOGIN user3 pass3
UPLOAD user3 file3.txt 100 /tmp/test3.txt
LIST user3
QUIT
```

5. Stop the server with Ctrl+C

6. Check the report:
```bash
cat reports/tsan_report.txt
```

**Expected Result**:
```
ThreadSanitizer: no issues found
```

---

### Test 3: Concurrent Access Test

**Purpose**: Verify proper handling of concurrent operations on same user

**Setup**:
1. Start server:
```bash
./server 9090
```

2. Create test files:
```bash
echo "test content 1" > /tmp/test1.txt
echo "test content 2" > /tmp/test2.txt
echo "test content 3" > /tmp/test3.txt
```

**Test Scenario**: Multiple clients accessing same user

**Terminal 2 - Client 1:**
```bash
./client 127.0.0.1 9090
```
Commands:
```
SIGNUP testuser testpass
LOGIN testuser testpass
UPLOAD testuser file1.txt 15 /tmp/test1.txt
LIST testuser
```
(Keep this client connected)

**Terminal 3 - Client 2:**
```bash
./client 127.0.0.1 9090
```
Commands:
```
LOGIN testuser testpass
UPLOAD testuser file2.txt 15 /tmp/test2.txt
LIST testuser
```
(Keep this client connected)

**Terminal 4 - Client 3:**
```bash
./client 127.0.0.1 9090
```
Commands:
```
LOGIN testuser testpass
UPLOAD testuser file3.txt 15 /tmp/test3.txt
LIST testuser
DOWNLOAD testuser file1.txt
DELETE testuser file2.txt
LIST testuser
```

**Expected Results**:
- All operations complete successfully
- No corrupted data
- LIST shows correct files after operations
- No server crashes
- All clients receive proper responses

---

### Test 4: Priority System Test

**Purpose**: Verify priority queue is working correctly

**Setup**:
1. Start server with verbose logging:
```bash
./server 9090
```

2. In another terminal, send commands rapidly:
```bash
./client 127.0.0.1 9090
```

Commands (send these quickly):
```
SIGNUP user1 pass1
LIST user1
UPLOAD user1 file1.txt 100 /tmp/test.txt
LOGIN user1 pass1
DELETE user1 file1.txt
DOWNLOAD user1 file1.txt
```

**Expected Server Output**:
You should see tasks processed in priority order:
```
Task submitted: SIGNUP (priority: HIGH)
Task submitted: LIST (priority: LOW)
Task submitted: UPLOAD (priority: NORMAL)
Task submitted: LOGIN (priority: HIGH)
Task submitted: DELETE (priority: HIGH)
Task submitted: DOWNLOAD (priority: NORMAL)

[Worker ...] Processing task: SIGNUP (priority: HIGH, user: user1)
[Worker ...] Processing task: LOGIN (priority: HIGH, user: user1)
[Worker ...] Processing task: DELETE (priority: HIGH, user: user1)
[Worker ...] Processing task: UPLOAD (priority: NORMAL, user: user1)
[Worker ...] Processing task: DOWNLOAD (priority: NORMAL, user: user1)
[Worker ...] Processing task: LIST (priority: LOW, user: user1)
```

Notice HIGH priority tasks are processed before NORMAL, and NORMAL before LOW.

---

### Test 5: Encoding/Decoding Test

**Purpose**: Verify files are encoded on upload and decoded on download

**Steps**:

1. Create a test file with known content:
```bash
echo "Hello, this is a test file for encoding!" > /tmp/original.txt
```

2. Start server:
```bash
./server 9090
```

3. Upload the file:
```bash
./client 127.0.0.1 9090
```
Commands:
```
SIGNUP testuser testpass
LOGIN testuser testpass
UPLOAD testuser encoded.txt 42 /tmp/original.txt
QUIT
```

4. Check the stored file is encoded (should NOT be readable):
```bash
cat storage/testuser/encoded.txt
```
Expected: Garbled/encoded text (not the original)

5. Download and verify decoding:
```bash
./client 127.0.0.1 9090
```
Commands:
```
LOGIN testuser testpass
DOWNLOAD testuser encoded.txt
QUIT
```

6. Check downloaded file:
```bash
# The client will show the path to downloaded file
cat /tmp/os_lab7_downloads/testuser_encoded.txt_*
```
Expected: "Hello, this is a test file for encoding!" (original content restored)

---

### Test 6: No Busy Waiting Test

**Purpose**: Verify client threads don't busy-wait

**Steps**:

1. Start server:
```bash
./server 9090
```

2. Connect a client but don't send commands:
```bash
./client 127.0.0.1 9090
```

3. In another terminal, check CPU usage:
```bash
top -p $(pgrep server)
```

**Expected Result**:
- Server CPU usage should be near 0% when idle
- No spinning/busy-waiting threads
- CPU usage only increases when processing commands

---

### Test 7: Stress Test

**Purpose**: Test server under load

**Setup Script** (save as `stress_test.sh`):
```bash
#!/bin/bash

# Function to run a client
run_client() {
    local id=$1
    ./client 127.0.0.1 9090 << EOF
SIGNUP user${id} pass${id}
LOGIN user${id} pass${id}
UPLOAD user${id} file${id}.txt 100 /tmp/test.txt
LIST user${id}
DOWNLOAD user${id} file${id}.txt
DELETE user${id} file${id}.txt
QUIT
EOF
}

# Create test file
echo "test content" > /tmp/test.txt

# Run 10 clients in parallel
for i in {1..10}; do
    run_client $i &
done

# Wait for all clients to finish
wait

echo "Stress test completed"
```

**Run**:
```bash
chmod +x stress_test.sh
./stress_test.sh
```

**Expected Result**:
- All clients complete successfully
- No server crashes
- No deadlocks
- All operations return correct results

---

## Common Issues and Solutions

### Issue 1: "Address already in use"
**Solution**: Wait a few seconds for the port to be released, or use a different port:
```bash
./server 9091
```

### Issue 2: SQLite database locked
**Solution**: Remove the database file:
```bash
rm storage/users.db
```

### Issue 3: Permission denied on storage directory
**Solution**: Ensure storage directory exists and is writable:
```bash
mkdir -p storage
chmod 755 storage
```

### Issue 4: Client can't connect
**Solution**: 
- Verify server is running: `ps aux | grep server`
- Check firewall settings
- Try localhost instead of 127.0.0.1

### Issue 5: Valgrind shows "still reachable" blocks
**Solution**: This is normal for SQLite and pthread libraries. Focus on "definitely lost" and "indirectly lost" which should be 0.

---

## Test Checklist

Before submission, verify:

- [ ] `make clean && make` completes without errors
- [ ] Server starts and accepts connections
- [ ] Client can connect and execute all commands
- [ ] SIGNUP creates new users
- [ ] LOGIN authenticates users
- [ ] UPLOAD stores files (encoded)
- [ ] DOWNLOAD retrieves files (decoded)
- [ ] DELETE removes files
- [ ] LIST shows correct files
- [ ] Valgrind shows no definite memory leaks
- [ ] ThreadSanitizer shows no race conditions
- [ ] Multiple concurrent clients work correctly
- [ ] Priority system processes high-priority tasks first
- [ ] No busy waiting (low CPU when idle)
- [ ] Stress test with 10+ clients succeeds

---

## Performance Benchmarks

Expected performance on a typical system:

- **Throughput**: 100+ operations/second
- **Latency**: < 50ms per operation
- **Concurrent Clients**: 50+ simultaneous connections
- **Memory Usage**: < 50MB with 10 clients
- **CPU Usage**: < 5% when idle, scales with load

---

## Debugging Tips

### Enable Verbose Logging
The server already prints task submissions and completions. To add more logging:

1. Check worker logs in server output
2. Look for error messages in client output
3. Use `strace` to trace system calls:
```bash
strace -f ./server 9090
```

### Debug Deadlocks
If server hangs:
```bash
# Get process ID
ps aux | grep server

# Attach GDB
gdb -p <PID>

# Show all threads
info threads

# Show backtrace for each thread
thread apply all bt
```

### Check File Encoding
```bash
# View hex dump of encoded file
hexdump -C storage/testuser/file.txt | head

# Compare with original
hexdump -C /tmp/original.txt | head
```

---

## Final Verification

Run all tests in sequence:

```bash
# 1. Compilation
make clean && make

# 2. Basic functionality
# (Manual test with client)

# 3. Memory leaks
./testing/run_valgrind_simple.sh

# 4. Race conditions
./testing/run_tsan_simple.sh

# 5. Stress test
./stress_test.sh
```

If all tests pass, your implementation is complete and ready for submission!
