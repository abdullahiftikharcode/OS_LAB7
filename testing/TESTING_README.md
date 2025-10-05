# Testing for Memory Leaks and Race Conditions

## Quick Start

### 1. Memory Leak Test (Valgrind)
```bash
chmod +x run_valgrind_simple.sh
./run_valgrind_simple.sh
```
Then in another terminal: `./client_debug` → perform operations → Ctrl+C server

### 2. Race Condition Test (ThreadSanitizer - Fully Automated!)
```bash
chmod +x run_tsan_simple.sh
./run_tsan_simple.sh
```
**Fully automated!** Runs 4 stress tests with concurrent operations automatically.

## Documentation

- **`HOW_TO_TEST.md`** - Step-by-step guide with examples (START HERE!)
- **`QUICK_TEST.md`** - Quick reference card
- **`TESTING_GUIDE.md`** - Comprehensive testing guide

## Test Scripts

- **`run_valgrind_simple.sh`** - Interactive Valgrind test
- **`run_tsan_simple.sh`** - Interactive ThreadSanitizer test

## Makefile Targets

```bash
make debug    # Build for Valgrind testing
make tsan     # Build for ThreadSanitizer testing
make clean    # Clean all builds
```

## Pass Criteria

✅ **Valgrind:** 0 bytes definitely lost, 0 bytes indirectly lost  
✅ **ThreadSanitizer:** No "WARNING: ThreadSanitizer" messages

---

**Read `HOW_TO_TEST.md` for detailed instructions!**
