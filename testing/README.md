# Testing Directory

This directory contains all testing scripts and documentation for the multi-threaded file storage server.

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
