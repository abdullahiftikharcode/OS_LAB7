# Project Organization

## Directory Structure

```
OS_LAB7/
├── include/              # Header files
│   ├── client.h
│   ├── fs.h
│   ├── queue.h
│   ├── server.h
│   ├── types.h
│   ├── user.h
│   └── worker.h
│
├── src/                  # Source files
│   ├── client.c
│   ├── client_program.c
│   ├── fs.c
│   ├── main.c
│   ├── queue.c
│   ├── user.c
│   └── worker.c
│
├── storage/              # User file storage
│   ├── abdullah/
│   └── testuser/
│
├── testing/              # Testing scripts and documentation
│   ├── README.md                    # Testing overview
│   ├── HOW_TO_TEST.md              # Step-by-step guide
│   ├── QUICK_TEST.md               # Quick reference
│   ├── TESTING_GUIDE.md            # Comprehensive guide
│   ├── TESTING_README.md           # Testing summary
│   ├── RACE_CONDITION_FIX.md       # Race condition fix documentation
│   ├── run_valgrind_simple.sh      # Valgrind test script
│   ├── run_tsan_simple.sh          # ThreadSanitizer test script
│   └── test_client.sh              # Client test script
│
├── reports/              # Test reports (auto-generated)
│   ├── README.md                    # Reports documentation
│   ├── valgrind_report.txt         # Valgrind output
│   └── tsan_report.txt*            # ThreadSanitizer output
│
├── Makefile              # Build configuration
├── README.md             # Project overview
├── CLIENT_README.md      # Client documentation
├── CODE_WALKTHROUGH.md   # Code explanation
├── EXECUTION_FLOW.md     # Execution flow documentation
└── .gitignore            # Git ignore rules
```

## Quick Reference

### Building

```bash
make                # Build server and client
make debug          # Build with debug symbols (for Valgrind)
make tsan           # Build with ThreadSanitizer
make clean          # Clean all builds
```

### Running

```bash
./server [port]           # Start server (default port 9090)
./client [host] [port]    # Start client
```

### Testing

```bash
# From project root:
./testing/run_valgrind_simple.sh    # Memory leak test
./testing/run_tsan_simple.sh        # Race condition test
```

### Reports

All test reports are saved in `reports/` directory:
- `reports/valgrind_report.txt` - Memory leak report
- `reports/tsan_report.txt*` - Race condition reports

## Documentation

### Main Documentation
- `README.md` - Project overview and quick start
- `CLIENT_README.md` - Client usage guide
- `CODE_WALKTHROUGH.md` - Detailed code explanation
- `EXECUTION_FLOW.md` - How the system works

### Testing Documentation
- `testing/README.md` - Testing overview (START HERE)
- `testing/HOW_TO_TEST.md` - Step-by-step testing guide
- `testing/QUICK_TEST.md` - Quick reference card
- `testing/TESTING_GUIDE.md` - Comprehensive testing guide
- `testing/RACE_CONDITION_FIX.md` - Race condition fix details

### Reports Documentation
- `reports/README.md` - How to read test reports

## File Organization Benefits

### ✅ Before vs After

**Before:**
```
OS_LAB7/
├── run_valgrind_simple.sh
├── run_tsan_simple.sh
├── test_client.sh
├── HOW_TO_TEST.md
├── TESTING_GUIDE.md
├── QUICK_TEST.md
├── valgrind_report.txt
├── tsan_report.txt.1234
├── tsan_report.txt.5678
└── ... (mixed with source files)
```

**After:**
```
OS_LAB7/
├── testing/              # All test scripts and docs
├── reports/              # All test reports
└── ... (clean root directory)
```

### Benefits

1. **Clean Root Directory** - Easy to find main project files
2. **Organized Testing** - All testing materials in one place
3. **Separate Reports** - Test outputs don't clutter the project
4. **Easy Navigation** - Clear structure for new contributors
5. **Git-Friendly** - Easy to ignore reports while keeping structure

## Workflow

### Development
1. Edit code in `src/` and `include/`
2. Build with `make`
3. Test with `./server` and `./client`

### Testing
1. Run `./testing/run_valgrind_simple.sh` for memory leaks
2. Run `./testing/run_tsan_simple.sh` for race conditions
3. Check reports in `reports/` directory

### CI/CD
```bash
# In your CI pipeline
make clean
./testing/run_valgrind_simple.sh || exit 1
./testing/run_tsan_simple.sh || exit 1
echo "All tests passed!"
```

---

**This organization makes the project professional and easy to maintain!**
