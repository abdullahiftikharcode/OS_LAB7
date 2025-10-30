# Quick Test Guide - OS Lab 7

## 🚀 Run All Tests in One Command

### Option 1: Python Test Script (Recommended)
```bash
python3 testing/comprehensive_test.py
```

**This tests:**
- ✅ Compilation (all build types)
- ✅ Server startup/shutdown
- ✅ User registration (SIGNUP)
- ✅ User authentication (LOGIN)
- ✅ File operations (UPLOAD, DOWNLOAD, DELETE, LIST)
- ✅ Concurrent operations (thread safety)
- ✅ Encoding/Decoding (BONUS)
- ✅ Priority system (BONUS)
- ✅ Stress test (20+ clients)
- ✅ Error handling

**Duration:** ~30 seconds

---

### Option 2: Bash Test Script
```bash
chmod +x testing/comprehensive_test.sh
./testing/comprehensive_test.sh
```

**Requirements:** Bash, netcat (nc)

---

## 🔍 Memory Leak Test (Valgrind)

```bash
chmod +x testing/run_valgrind_simple.sh
./testing/run_valgrind_simple.sh
```

**What to do:**
1. Script builds and starts server with Valgrind
2. Press Enter when prompted
3. In another terminal, run: `./client 127.0.0.1 9090`
4. Perform operations (SIGNUP, LOGIN, UPLOAD, etc.)
5. Type `QUIT` in client
6. Press Ctrl+C to stop server
7. Check report: `cat reports/valgrind_report.txt`

**Expected:** 0 bytes definitely lost, 0 bytes indirectly lost

---

## 🧵 Race Condition Test (ThreadSanitizer)

```bash
chmod +x testing/run_tsan_simple.sh
./testing/run_tsan_simple.sh
```

**Fully automated** - runs concurrent stress tests and reports results.

**Expected:** "✓ SUCCESS: No race conditions detected!"

**Duration:** ~60 seconds

---

## 📊 Expected Output

### ✅ All Tests Pass
```
========================================
TEST SUMMARY
========================================

Total Tests: 45
Passed: 45
Failed: 0

✓ ALL TESTS PASSED!

Your implementation meets all requirements:
  ✓ Multi-threaded server with user management
  ✓ Simple client communication
  ✓ All file operations (UPLOAD, DOWNLOAD, DELETE, LIST)
  ✓ Two thread-safe queues (Client Queue, Task Queue)
  ✓ Two thread pools (Client Pool, Worker Pool)
  ✓ Concurrent access handling
  ✓ Worker-to-client communication
  ✓ BONUS: No busy waiting
  ✓ BONUS: Encoding/Decoding
  ✓ BONUS: Priority system
```

---

## 🐛 Troubleshooting

### "Address already in use"
```bash
pkill -9 server
# Wait 2 seconds, then retry
```

### "Connection refused"
```bash
# Check if server is running
ps aux | grep server

# If not, start it manually
./server 9090
```

### "Permission denied" on scripts
```bash
chmod +x testing/*.sh
chmod +x testing/*.py
```

### Python not found
```bash
# Install Python 3
sudo apt-get install python3  # Ubuntu/Debian
brew install python3          # macOS
```

---

## 📝 Manual Testing

If automated tests fail, test manually:

**Terminal 1:**
```bash
make clean && make
./server 9090
```

**Terminal 2:**
```bash
./client 127.0.0.1 9090
```

**Commands to test:**
```
SIGNUP testuser testpass
LOGIN testuser testpass
UPLOAD testuser file.txt 100 /tmp/test.txt
LIST testuser
DOWNLOAD testuser file.txt
DELETE testuser file.txt
QUIT
```

---

## ✅ Pre-Submission Checklist

Run these commands and verify all pass:

```bash
# 1. Comprehensive functional test
python3 testing/comprehensive_test.py
# Expected: All tests passed

# 2. Memory leak test
./testing/run_valgrind_simple.sh
# Expected: 0 bytes definitely lost

# 3. Race condition test
./testing/run_tsan_simple.sh
# Expected: No race conditions detected
```

---

## 📁 Test Reports Location

All reports saved in: `reports/`
- `valgrind_report.txt` - Memory leak report
- `tsan_report.txt*` - Race condition reports

---

## 🎯 What Each Test Verifies

| Test | Verifies |
|------|----------|
| Compilation | Makefile works, all builds succeed |
| Server Startup | Server listens on port, accepts connections |
| SIGNUP | User registration, duplicate prevention |
| LOGIN | Authentication, password validation |
| UPLOAD | File storage, encoding, quota checking |
| DOWNLOAD | File retrieval, decoding |
| DELETE | File removal |
| LIST | Directory listing |
| Concurrent Ops | Thread safety, no crashes |
| Encoding/Decoding | XOR cipher works correctly |
| Priority System | High priority tasks processed first |
| Stress Test | Handles 20+ concurrent clients |
| Valgrind | No memory leaks |
| ThreadSanitizer | No race conditions |

---

## 💡 Tips

1. **Run Python test first** - fastest and most comprehensive
2. **Run Valgrind and TSAN** - required for full verification
3. **Check server logs** - see task priority ordering
4. **Review reports/** - detailed error information if tests fail

---

## 🆘 Need Help?

See detailed guides:
- `testing/README.md` - Complete testing documentation
- `TESTING_GUIDE.md` - Comprehensive testing guide
- `REQUIREMENTS_CHECKLIST.md` - Requirements verification
- `DESIGN.md` - System design and architecture
