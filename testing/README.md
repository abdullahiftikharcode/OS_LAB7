# Testing Directory

This directory contains comprehensive test scripts and utilities for the OS Lab 7 project.

## Quick Start

### Run All Tests (Recommended)
```bash
# Python version (more reliable, cross-platform)
python3 testing/comprehensive_test.py

# Bash version (Linux/macOS/WSL)
chmod +x testing/comprehensive_test.sh
./testing/comprehensive_test.sh
```

### Individual Tests

**Memory Leak Testing (Valgrind):**
```bash
chmod +x testing/run_valgrind_simple.sh
./testing/run_valgrind_simple.sh
```

**Race Condition Testing (ThreadSanitizer):**
```bash
chmod +x testing/run_tsan_simple.sh
./testing/run_tsan_simple.sh
```

## Test Scripts

### comprehensive_test.py
**Recommended for all platforms**

Comprehensive Python test script that verifies:
- Compilation (standard, debug, tsan builds)
- Server startup and shutdown
- User registration (SIGNUP)
- User authentication (LOGIN)
- File upload (UPLOAD) with encoding
- File listing (LIST)
- File download (DOWNLOAD) with decoding
- File deletion (DELETE)
- Concurrent operations (thread safety)
- Encoding/Decoding (BONUS)
- Priority system (BONUS)
- Stress testing (20+ concurrent clients)
- Error handling

**Usage:**
```bash
python3 testing/comprehensive_test.py
```

**Requirements:**
- Python 3.6+
- Server must be built (`make`)

### comprehensive_test.sh
Bash version of comprehensive test script.

**Usage:**
```bash
chmod +x testing/comprehensive_test.sh
./testing/comprehensive_test.sh
```

**Requirements:**
- Bash shell
- nc (netcat) command
- Standard Unix utilities

### run_valgrind_simple.sh
Tests for memory leaks using Valgrind.

**What it tests:**
- Memory allocation/deallocation
- Memory leaks (definitely lost, indirectly lost)
- Invalid memory access
- Use of uninitialized values

**Expected Result:**
```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
ERROR SUMMARY: 0 errors from 0 contexts
```

**Usage:**
```bash
./testing/run_valgrind_simple.sh
```

### run_tsan_simple.sh
Tests for race conditions using ThreadSanitizer.

**What it tests:**
- Data races between threads
- Deadlocks
- Thread synchronization issues
- Concurrent access problems

**Expected Result:**
```
 SUCCESS: No race conditions detected!
```

**Usage:**
```bash
./testing/run_tsan_simple.sh
```

### test_client.sh
Basic functional test script.

**Usage:**
```bash
chmod +x testing/test_client.sh
./testing/test_client.sh
```

### tsan_test_client.py
Python helper for ThreadSanitizer testing.

## Test Reports

All test reports are saved in the `reports/` directory:
- `valgrind_report.txt` - Valgrind memory leak report
- `tsan_report.txt*` - ThreadSanitizer race condition reports

## Testing Checklist

Before submission, ensure all tests pass:

- [ ] `python3 testing/comprehensive_test.py` - All tests pass
- [ ] `./testing/run_valgrind_simple.sh` - No memory leaks
- [ ] `./testing/run_tsan_simple.sh` - No race conditions
- [ ] Server handles 20+ concurrent clients
- [ ] All file operations work correctly
- [ ] Priority system orders tasks correctly
- [ ] Files are encoded/decoded properly

## Troubleshooting

### "Address already in use"
Wait a few seconds for the port to be released, or kill existing server:
```bash
pkill -9 server
```

### "Connection refused"
Ensure server is running:
```bash
ps aux | grep server
```

### Python tests fail with "ModuleNotFoundError"
Ensure Python 3 is installed:
```bash
python3 --version
```

### Valgrind shows "still reachable" blocks
This is normal for SQLite and pthread libraries. Focus on "definitely lost" which should be 0.

### ThreadSanitizer shows warnings
Review the specific warning in `reports/tsan_report.txt*` and fix the synchronization issue.

## Manual Testing

If automated tests fail, you can test manually:

**Terminal 1 - Server:**
```bash
./server 9090
```

**Terminal 2 - Client:**
```bash
./client 127.0.0.1 9090
```

Then enter commands:
```
SIGNUP testuser testpass
LOGIN testuser testpass
UPLOAD testuser file.txt 100 /tmp/test.txt
LIST testuser
DOWNLOAD testuser file.txt
DELETE testuser file.txt
QUIT
```

## Performance Benchmarks

Expected performance on typical hardware:
- **Throughput**: 100+ operations/second
- **Latency**: < 50ms per operation
- **Concurrent Clients**: 50+ simultaneous
- **Memory Usage**: < 50MB with 10 clients
- **CPU Usage**: < 5% when idleaded file storage server.

## Quick Start

```bash
# From project root, run:

# 1. Test for memory leaks
./testing/run_valgrind_simple.sh

# 2. Test for race conditions (fully automated)
./testing/run_tsan_simple.sh
```

## Directory Structure

```
testing/
├── README.md                    # This file
├── HOW_TO_TEST.md              # Detailed step-by-step testing guide
├── QUICK_TEST.md               # Quick reference card
├── TESTING_GUIDE.md            # Comprehensive testing guide
├── TESTING_README.md           # Testing overview
├── RACE_CONDITION_FIX.md       # Documentation of race condition fix
├── run_valgrind_simple.sh      # Valgrind memory leak test
├── run_tsan_simple.sh          # ThreadSanitizer race condition test
└── test_client.sh              # Client test script

../reports/                      # Test reports are saved here
├── valgrind_report.txt         # Valgrind output
└── tsan_report.txt*            # ThreadSanitizer output
```

## Test Scripts

### `run_valgrind_simple.sh` - Memory Leak Detection
Semi-automated test that checks for memory leaks using Valgrind.

**Usage:**
```bash
./testing/run_valgrind_simple.sh
# Then in another terminal: ./client_debug
# Perform operations, quit client, Ctrl+C server
```

**Output:** `reports/valgrind_report.txt`

**Pass Criteria:** 
- definitely lost: 0 bytes
- indirectly lost: 0 bytes

### `run_tsan_simple.sh` - Race Condition Detection
Fully automated test that checks for race conditions using ThreadSanitizer.

**Usage:**
```bash
./testing/run_tsan_simple.sh
```

**Output:** `reports/tsan_report.txt*`

**Pass Criteria:** 
- No "WARNING: ThreadSanitizer" messages

**Duration:** ~30-60 seconds

**What it does:**
- Runs 4 stress tests with concurrent operations
- Test 1: 10 clients with full operations (5 batches of 2)
- Test 2: 20 rapid-fire same-user operations (4 batches of 5)
- Test 3: 15 mixed concurrent operations (5 batches of 3)
- Test 4: 30 rapid connect/disconnect cycles (6 batches of 5)

## Documentation

- **Start Here:** `HOW_TO_TEST.md` - Complete step-by-step guide
- **Quick Reference:** `QUICK_TEST.md` - Commands and expected output
- **Comprehensive:** `TESTING_GUIDE.md` - Detailed testing information
- **Race Condition Fix:** `RACE_CONDITION_FIX.md` - How we fixed the detected race

## Requirements

```bash
# Valgrind
sudo apt-get install valgrind

# ThreadSanitizer (usually built-in with GCC >= 4.8)
gcc --version

# Netcat (for automated tests)
sudo apt-get install netcat
```

## Makefile Targets

From project root:

```bash
make debug    # Build for Valgrind testing
make tsan     # Build for ThreadSanitizer testing
make clean    # Clean all builds
```

## Expected Results

### ✅ Pass - No Issues
```
Valgrind:
  definitely lost: 0 bytes in 0 blocks
  indirectly lost: 0 bytes in 0 blocks

ThreadSanitizer:
  ✓ SUCCESS: No race conditions detected!
```

### ✗ Fail - Issues Detected
Check the report files in `reports/` directory for details.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Scripts won't run | `chmod +x testing/*.sh` |
| Can't find reports | Check `reports/` directory |
| Tests hang | Server connection queue full, tests run in batches now |
| TSAN not available | Use GCC >= 4.8 or Clang >= 3.2 |

## CI/CD Integration

```bash
# Add to your CI pipeline
cd /path/to/project
./testing/run_valgrind_simple.sh || exit 1
./testing/run_tsan_simple.sh || exit 1
echo "All tests passed!"
```

---

**For detailed instructions, see `HOW_TO_TEST.md`**
