#!/bin/bash
# Automated ThreadSanitizer test with concurrent operations

set -e

# Change to project root directory
cd "$(dirname "$0")/.." || exit 1

echo "=========================================="
echo "Race Condition Testing with ThreadSanitizer"
echo "=========================================="

# Check if python3 is available
if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 is not installed!"
    echo "Install it with: sudo apt-get install python3"
    exit 1
fi

# Make test client executable
chmod +x testing/tsan_test_client.py 2>/dev/null || true

# Build with ThreadSanitizer
echo ""
echo "Building with ThreadSanitizer..."
make clean
make tsan

# Create test files
echo "Creating test files..."
for i in {1..10}; do
    echo "Test file content for user $i" > test_file_$i.txt
done

# Set ThreadSanitizer options
export TSAN_OPTIONS="log_path=reports/tsan_report.txt second_deadlock_stack=1"

# Start server in background
echo ""
echo "Starting server with ThreadSanitizer..."
./server_tsan 9090 &
SERVER_PID=$!
sleep 2

# Check if server started
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start!"
    if [ -f reports/tsan_report.txt ]; then
        cat reports/tsan_report.txt*
    fi
    exit 1
fi

echo "Server started (PID: $SERVER_PID)"
echo ""
echo "Running concurrent stress tests..."

# Function to simulate a client performing operations
run_client_operations() {
    local USER=$1
    local NUM=$2
    
    # Use Python client for reliable connection handling
    python3 testing/tsan_test_client.py "SIGNUP ${USER}${NUM} pass${NUM}" > /dev/null 2>&1 || true
    python3 testing/tsan_test_client.py "LOGIN ${USER}${NUM} pass${NUM}" > /dev/null 2>&1 || true
    python3 testing/tsan_test_client.py "LIST ${USER}${NUM}" > /dev/null 2>&1 || true
}

# Test 1: Sequential clients with some concurrency
echo "Test 1: Running 5 clients sequentially..."
for i in {1..5}; do
    echo "  Client $i..."
    run_client_operations "user" $i
done
echo "  Test 1 complete!"
sleep 0.5

# Test 2: Concurrent signups (stress test for races)
echo "Test 2: Testing concurrent signups..."
for batch in {1..3}; do
    echo "  Batch $batch..."
    PIDS=""
    for i in {1..2}; do
        python3 testing/tsan_test_client.py "SIGNUP user${batch}${i} pass${batch}${i}" &
        PIDS="$PIDS $!"
    done
    echo "  Waiting for batch $batch to complete..."
    for pid in $PIDS; do
        wait $pid
    done
    echo "  Batch $batch done"
    sleep 0.2
done
echo "  Test 2 complete!"
sleep 0.3

# Test 3: Concurrent logins (stress test for races)
echo "Test 3: Testing concurrent logins..."
# Create users first
for i in {1..6}; do
    python3 testing/tsan_test_client.py "SIGNUP loginuser${i} pass${i}" > /dev/null 2>&1 || true
done
sleep 0.5

# Now test concurrent logins
for batch in {1..3}; do
    echo "  Batch $batch..."
    PIDS=""
    for i in {1..2}; do
        python3 testing/tsan_test_client.py "LOGIN loginuser${batch}${i} pass${batch}${i}" &
        PIDS="$PIDS $!"
    done
    for pid in $PIDS; do
        wait $pid
    done
    sleep 0.2
done
echo "  Test 3 complete!"
sleep 0.3

# Test 4: Concurrent LIST operations
echo "Test 4: Testing concurrent LIST operations..."
# Create a user for listing
python3 testing/tsan_test_client.py "SIGNUP listuser listpass" > /dev/null 2>&1 || true
sleep 0.2

# Test concurrent LISTs
for batch in {1..3}; do
    echo "  Batch $batch..."
    PIDS=""
    for i in {1..2}; do
        python3 testing/tsan_test_client.py "LIST listuser" &
        PIDS="$PIDS $!"
    done
    for pid in $PIDS; do
        wait $pid
    done
    sleep 0.2
done
echo "  Test 4 complete!"
sleep 0.3

echo ""
echo "All stress tests completed!"
echo "Stopping server..."

# Stop server gracefully
kill -INT $SERVER_PID 2>/dev/null || true
sleep 2

# If still running, force kill
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "Force killing server..."
    kill -9 $SERVER_PID 2>/dev/null || true
fi

# Wait for TSAN to finish writing
sleep 1

# Analyze results
echo ""
echo "=========================================="
echo "ThreadSanitizer Results"
echo "=========================================="
echo ""

# Check if any TSAN reports were generated
if ls reports/tsan_report.txt* 1> /dev/null 2>&1; then
    # Check for race conditions
    if grep -q "WARNING: ThreadSanitizer: data race" reports/tsan_report.txt* 2>/dev/null; then
        echo "✗ FAILURE: Race conditions detected!"
        echo ""
        echo "Race condition details:"
        grep -A 15 "WARNING: ThreadSanitizer: data race" reports/tsan_report.txt* | head -100
        echo ""
        echo "See reports/tsan_report.txt* files for full details"
        
        # Cleanup
        rm -f test_file_*.txt /tmp/test_upload_*.txt /tmp/stress_*.txt
        exit 1
    elif grep -q "WARNING: ThreadSanitizer:" reports/tsan_report.txt* 2>/dev/null; then
        echo "✗ FAILURE: ThreadSanitizer warnings detected!"
        echo ""
        cat reports/tsan_report.txt*
        
        # Cleanup
        rm -f test_file_*.txt /tmp/test_upload_*.txt /tmp/stress_*.txt
        exit 1
    else
        echo "✓ SUCCESS: No race conditions detected!"
        echo ""
        echo "ThreadSanitizer summary:"
        tail -20 reports/tsan_report.txt* 2>/dev/null || echo "No issues found"
    fi
else
    echo "✓ SUCCESS: No race conditions detected!"
    echo "(No ThreadSanitizer reports generated)"
fi

echo ""
echo "Full reports saved to: reports/tsan_report.txt*"
echo ""

# Cleanup
rm -f test_file_*.txt /tmp/test_upload_*.txt /tmp/stress_*.txt

echo "=========================================="
echo "Testing Complete!"
echo "=========================================="
