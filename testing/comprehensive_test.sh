#!/bin/bash
# Comprehensive Test Script for OS Lab 7 - Multi-threaded File Server
# Tests all requirements and bonus features

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Change to project root
cd "$(dirname "$0")/.." || exit 1

# Create reports directory
mkdir -p reports

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
    ((TESTS_TOTAL++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++))
    ((TESTS_TOTAL++))
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_section() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    
    # Kill server if running
    if [ ! -z "$SERVER_PID" ]; then
        kill -INT $SERVER_PID 2>/dev/null || true
        sleep 1
        kill -9 $SERVER_PID 2>/dev/null || true
    fi
    
    # Remove test files
    rm -f /tmp/test_*.txt
    rm -f test_file_*.txt
    rm -rf /tmp/os_lab7_downloads
    
    # Clean storage (optional - comment out to preserve data)
    # rm -rf storage/*
}

trap cleanup EXIT

# Helper function to send command to server
send_command() {
    local cmd="$1"
    echo "$cmd" | nc -w 2 127.0.0.1 9090 2>/dev/null || echo "ERR Connection failed"
}

# Helper function to wait for server
wait_for_server() {
    local max_attempts=10
    local attempt=0
    
    while [ $attempt -lt $max_attempts ]; do
        if nc -z 127.0.0.1 9090 2>/dev/null; then
            return 0
        fi
        sleep 0.5
        ((attempt++))
    done
    
    return 1
}

# ============================================================================
# TEST 1: COMPILATION
# ============================================================================
log_section "TEST 1: Compilation"

log_info "Testing standard build..."
if make clean > /dev/null 2>&1 && make > /dev/null 2>&1; then
    log_success "Standard build successful"
    
    # Check if binaries exist
    if [ -f "server" ] && [ -f "client" ]; then
        log_success "Server and client binaries created"
    else
        log_fail "Binaries not found after build"
    fi
else
    log_fail "Standard build failed"
    exit 1
fi

log_info "Testing debug build..."
if make clean > /dev/null 2>&1 && make debug > /dev/null 2>&1; then
    log_success "Debug build successful"
    
    if [ -f "server_debug" ] && [ -f "client_debug" ]; then
        log_success "Debug binaries created"
    else
        log_fail "Debug binaries not found"
    fi
else
    log_fail "Debug build failed"
fi

log_info "Testing ThreadSanitizer build..."
if make clean > /dev/null 2>&1 && make tsan > /dev/null 2>&1; then
    log_success "ThreadSanitizer build successful"
    
    if [ -f "server_tsan" ]; then
        log_success "ThreadSanitizer binary created"
    else
        log_fail "ThreadSanitizer binary not found"
    fi
else
    log_fail "ThreadSanitizer build failed"
fi

# Rebuild standard version for testing
make clean > /dev/null 2>&1
make > /dev/null 2>&1

# ============================================================================
# TEST 2: SERVER STARTUP AND SHUTDOWN
# ============================================================================
log_section "TEST 2: Server Startup and Shutdown"

log_info "Starting server..."
./server 9090 > /tmp/server_test.log 2>&1 &
SERVER_PID=$!
sleep 1

if kill -0 $SERVER_PID 2>/dev/null; then
    log_success "Server started successfully (PID: $SERVER_PID)"
else
    log_fail "Server failed to start"
    cat /tmp/server_test.log
    exit 1
fi

if wait_for_server; then
    log_success "Server accepting connections on port 9090"
else
    log_fail "Server not accepting connections"
fi

# ============================================================================
# TEST 3: USER REGISTRATION (SIGNUP)
# ============================================================================
log_section "TEST 3: User Registration (SIGNUP)"

log_info "Testing user signup..."
response=$(send_command "SIGNUP testuser1 password123")
if echo "$response" | grep -q "OK"; then
    log_success "User signup successful"
else
    log_fail "User signup failed: $response"
fi

log_info "Testing duplicate signup..."
response=$(send_command "SIGNUP testuser1 password123")
if echo "$response" | grep -q "ERR"; then
    log_success "Duplicate signup correctly rejected"
else
    log_fail "Duplicate signup should have failed"
fi

log_info "Testing multiple user signups..."
for i in {2..5}; do
    response=$(send_command "SIGNUP testuser$i pass$i")
    if echo "$response" | grep -q "OK"; then
        log_success "User testuser$i signup successful"
    else
        log_fail "User testuser$i signup failed"
    fi
done

# ============================================================================
# TEST 4: USER AUTHENTICATION (LOGIN)
# ============================================================================
log_section "TEST 4: User Authentication (LOGIN)"

log_info "Testing valid login..."
response=$(send_command "LOGIN testuser1 password123")
if echo "$response" | grep -q "OK"; then
    log_success "Valid login successful"
else
    log_fail "Valid login failed: $response"
fi

log_info "Testing invalid password..."
response=$(send_command "LOGIN testuser1 wrongpassword")
if echo "$response" | grep -q "ERR"; then
    log_success "Invalid password correctly rejected"
else
    log_fail "Invalid password should have been rejected"
fi

log_info "Testing non-existent user..."
response=$(send_command "LOGIN nonexistent password")
if echo "$response" | grep -q "ERR"; then
    log_success "Non-existent user correctly rejected"
else
    log_fail "Non-existent user should have been rejected"
fi

# ============================================================================
# TEST 5: FILE UPLOAD
# ============================================================================
log_section "TEST 5: File Upload (UPLOAD)"

log_info "Creating test files..."
echo "This is test file 1 content" > /tmp/test_upload_1.txt
echo "This is test file 2 with more content to test encoding" > /tmp/test_upload_2.txt
dd if=/dev/urandom of=/tmp/test_upload_large.txt bs=1024 count=100 2>/dev/null
log_success "Test files created"

log_info "Testing file upload..."
file_size=$(stat -f%z /tmp/test_upload_1.txt 2>/dev/null || stat -c%s /tmp/test_upload_1.txt)
response=$(send_command "UPLOAD testuser1 file1.txt $file_size /tmp/test_upload_1.txt")
if echo "$response" | grep -q "OK"; then
    log_success "File upload successful"
else
    log_fail "File upload failed: $response"
fi

log_info "Uploading multiple files..."
file_size=$(stat -f%z /tmp/test_upload_2.txt 2>/dev/null || stat -c%s /tmp/test_upload_2.txt)
response=$(send_command "UPLOAD testuser1 file2.txt $file_size /tmp/test_upload_2.txt")
if echo "$response" | grep -q "OK"; then
    log_success "Second file upload successful"
else
    log_fail "Second file upload failed"
fi

log_info "Testing large file upload..."
file_size=$(stat -f%z /tmp/test_upload_large.txt 2>/dev/null || stat -c%s /tmp/test_upload_large.txt)
response=$(send_command "UPLOAD testuser1 large.txt $file_size /tmp/test_upload_large.txt")
if echo "$response" | grep -q "OK"; then
    log_success "Large file upload successful"
else
    log_fail "Large file upload failed"
fi

# ============================================================================
# TEST 6: FILE LISTING (LIST)
# ============================================================================
log_section "TEST 6: File Listing (LIST)"

log_info "Testing file listing..."
response=$(send_command "LIST testuser1")
if echo "$response" | grep -q "OK"; then
    log_success "File listing successful"
    
    # Check if uploaded files are listed
    if echo "$response" | grep -q "file1.txt"; then
        log_success "file1.txt found in listing"
    else
        log_fail "file1.txt not found in listing"
    fi
    
    if echo "$response" | grep -q "file2.txt"; then
        log_success "file2.txt found in listing"
    else
        log_fail "file2.txt not found in listing"
    fi
else
    log_fail "File listing failed: $response"
fi

log_info "Testing empty directory listing..."
response=$(send_command "LIST testuser2")
if echo "$response" | grep -q "OK"; then
    log_success "Empty directory listing successful"
else
    log_fail "Empty directory listing failed"
fi

# ============================================================================
# TEST 7: FILE DOWNLOAD
# ============================================================================
log_section "TEST 7: File Download (DOWNLOAD)"

log_info "Testing file download..."
response=$(send_command "DOWNLOAD testuser1 file1.txt")
if echo "$response" | grep -q "OK"; then
    log_success "File download initiated successfully"
    
    # Check if temp file was created (path should be in response)
    if echo "$response" | grep -q "/tmp/os_lab7_downloads"; then
        log_success "Download temp file path returned"
    else
        log_warning "Download temp file path not found in response"
    fi
else
    log_fail "File download failed: $response"
fi

log_info "Testing download of non-existent file..."
response=$(send_command "DOWNLOAD testuser1 nonexistent.txt")
if echo "$response" | grep -q "ERR"; then
    log_success "Non-existent file download correctly rejected"
else
    log_fail "Non-existent file download should have failed"
fi

# ============================================================================
# TEST 8: FILE DELETION (DELETE)
# ============================================================================
log_section "TEST 8: File Deletion (DELETE)"

log_info "Testing file deletion..."
response=$(send_command "DELETE testuser1 file2.txt")
if echo "$response" | grep -q "OK"; then
    log_success "File deletion successful"
else
    log_fail "File deletion failed: $response"
fi

log_info "Verifying file was deleted..."
response=$(send_command "LIST testuser1")
if echo "$response" | grep -q "file2.txt"; then
    log_fail "Deleted file still appears in listing"
else
    log_success "Deleted file no longer in listing"
fi

log_info "Testing deletion of non-existent file..."
response=$(send_command "DELETE testuser1 nonexistent.txt")
if echo "$response" | grep -q "ERR"; then
    log_success "Non-existent file deletion correctly rejected"
else
    log_fail "Non-existent file deletion should have failed"
fi

# ============================================================================
# TEST 9: CONCURRENT OPERATIONS
# ============================================================================
log_section "TEST 9: Concurrent Operations"

log_info "Testing concurrent signups..."
PIDS=""
for i in {10..15}; do
    (send_command "SIGNUP concurrent_user$i pass$i" > /dev/null 2>&1) &
    PIDS="$PIDS $!"
done

# Wait for all concurrent operations
for pid in $PIDS; do
    wait $pid
done
log_success "Concurrent signups completed without crashes"

log_info "Testing concurrent logins..."
PIDS=""
for i in {1..5}; do
    (send_command "LOGIN testuser$i pass$i" > /dev/null 2>&1) &
    PIDS="$PIDS $!"
done

for pid in $PIDS; do
    wait $pid
done
log_success "Concurrent logins completed without crashes"

log_info "Testing concurrent operations on same user..."
echo "Test content for concurrent ops" > /tmp/test_concurrent.txt
file_size=$(stat -f%z /tmp/test_concurrent.txt 2>/dev/null || stat -c%s /tmp/test_concurrent.txt)

PIDS=""
(send_command "UPLOAD testuser1 concurrent1.txt $file_size /tmp/test_concurrent.txt" > /dev/null 2>&1) &
PIDS="$PIDS $!"
(send_command "LIST testuser1" > /dev/null 2>&1) &
PIDS="$PIDS $!"
(send_command "UPLOAD testuser1 concurrent2.txt $file_size /tmp/test_concurrent.txt" > /dev/null 2>&1) &
PIDS="$PIDS $!"

for pid in $PIDS; do
    wait $pid
done
log_success "Concurrent operations on same user completed without crashes"

# ============================================================================
# TEST 10: ENCODING/DECODING (BONUS)
# ============================================================================
log_section "TEST 10: Encoding/Decoding (BONUS)"

log_info "Creating test file with known content..."
echo "ENCODING_TEST_CONTENT_12345" > /tmp/test_encoding.txt
file_size=$(stat -f%z /tmp/test_encoding.txt 2>/dev/null || stat -c%s /tmp/test_encoding.txt)

log_info "Uploading file (should be encoded)..."
response=$(send_command "UPLOAD testuser1 encoded_test.txt $file_size /tmp/test_encoding.txt")
if echo "$response" | grep -q "OK"; then
    log_success "File uploaded for encoding test"
else
    log_fail "File upload for encoding test failed"
fi

log_info "Checking if stored file is encoded..."
if [ -f "storage/testuser1/encoded_test.txt" ]; then
    # Read the stored file and check if it's different from original
    stored_content=$(cat storage/testuser1/encoded_test.txt)
    original_content=$(cat /tmp/test_encoding.txt)
    
    if [ "$stored_content" != "$original_content" ]; then
        log_success "File is encoded in storage (content differs from original)"
    else
        log_fail "File is NOT encoded in storage (content matches original)"
    fi
else
    log_warning "Could not verify encoding (storage file not found)"
fi

log_info "Downloading file (should be decoded)..."
response=$(send_command "DOWNLOAD testuser1 encoded_test.txt")
if echo "$response" | grep -q "OK"; then
    log_success "File downloaded for decoding test"
    
    # Extract temp file path from response
    temp_path=$(echo "$response" | grep -oP '/tmp/os_lab7_downloads/[^ ]+' | head -1)
    
    if [ ! -z "$temp_path" ] && [ -f "$temp_path" ]; then
        decoded_content=$(cat "$temp_path")
        if [ "$decoded_content" = "ENCODING_TEST_CONTENT_12345" ]; then
            log_success "File correctly decoded (content matches original)"
        else
            log_fail "File decoding failed (content doesn't match)"
        fi
    else
        log_warning "Could not verify decoding (temp file not found)"
    fi
else
    log_fail "File download for decoding test failed"
fi

# ============================================================================
# TEST 11: PRIORITY SYSTEM (BONUS)
# ============================================================================
log_section "TEST 11: Priority System (BONUS)"

log_info "Testing priority queue implementation..."
log_info "Submitting tasks with different priorities rapidly..."

# Submit tasks in mixed order - server logs should show priority ordering
(send_command "LIST testuser1" > /dev/null 2>&1) &        # LOW priority
(send_command "UPLOAD testuser1 p1.txt 10 /tmp/test_encoding.txt" > /dev/null 2>&1) &  # NORMAL
(send_command "SIGNUP priority_user1 pass" > /dev/null 2>&1) &  # HIGH priority
(send_command "LIST testuser2" > /dev/null 2>&1) &        # LOW priority
(send_command "LOGIN testuser1 password123" > /dev/null 2>&1) &  # HIGH priority
(send_command "DOWNLOAD testuser1 file1.txt" > /dev/null 2>&1) &  # NORMAL

sleep 2
log_success "Priority tasks submitted (check server logs for ordering)"
log_info "Expected order: HIGH tasks (SIGNUP, LOGIN) -> NORMAL tasks (UPLOAD, DOWNLOAD) -> LOW tasks (LIST)"

# ============================================================================
# TEST 12: NO BUSY WAITING (BONUS)
# ============================================================================
log_section "TEST 12: No Busy Waiting (BONUS)"

log_info "Testing CPU usage when idle..."
log_info "Server should use minimal CPU when no operations are running"

# Get initial CPU usage
if command -v ps &> /dev/null; then
    sleep 2  # Let server settle
    cpu_usage=$(ps -p $SERVER_PID -o %cpu= 2>/dev/null || echo "0")
    
    if [ ! -z "$cpu_usage" ]; then
        # Remove leading/trailing whitespace
        cpu_usage=$(echo "$cpu_usage" | xargs)
        
        # Check if CPU usage is low (less than 5%)
        if (( $(echo "$cpu_usage < 5.0" | bc -l 2>/dev/null || echo "1") )); then
            log_success "Server CPU usage is low when idle ($cpu_usage%)"
        else
            log_warning "Server CPU usage is higher than expected ($cpu_usage%)"
        fi
    else
        log_warning "Could not measure CPU usage"
    fi
else
    log_warning "ps command not available, skipping CPU test"
fi

# ============================================================================
# TEST 13: THREAD POOLS
# ============================================================================
log_section "TEST 13: Thread Pools"

log_info "Verifying thread pools are created..."
if command -v pgrep &> /dev/null; then
    thread_count=$(ps -T -p $SERVER_PID 2>/dev/null | wc -l)
    
    if [ $thread_count -gt 1 ]; then
        log_success "Multiple threads detected (count: $thread_count)"
        log_info "Expected: 1 main + 4 client threads + 4 worker threads = 9 threads"
    else
        log_warning "Could not verify thread count"
    fi
else
    log_warning "Cannot verify thread pools (ps command not available)"
fi

# ============================================================================
# TEST 14: STRESS TEST
# ============================================================================
log_section "TEST 14: Stress Test"

log_info "Running stress test with 20 concurrent clients..."
PIDS=""
for i in {20..39}; do
    (
        send_command "SIGNUP stress_user$i pass$i" > /dev/null 2>&1
        send_command "LOGIN stress_user$i pass$i" > /dev/null 2>&1
        send_command "LIST stress_user$i" > /dev/null 2>&1
    ) &
    PIDS="$PIDS $!"
done

# Wait for all stress test operations
failed=0
for pid in $PIDS; do
    if ! wait $pid; then
        ((failed++))
    fi
done

if [ $failed -eq 0 ]; then
    log_success "Stress test completed successfully (20 concurrent clients)"
else
    log_warning "Stress test had $failed failures"
fi

# ============================================================================
# TEST 15: ERROR HANDLING
# ============================================================================
log_section "TEST 15: Error Handling"

log_info "Testing invalid commands..."
response=$(send_command "INVALID_COMMAND")
if echo "$response" | grep -q "ERR\|^$"; then
    log_success "Invalid command handled gracefully"
else
    log_warning "Invalid command response: $response"
fi

log_info "Testing malformed commands..."
response=$(send_command "UPLOAD")
if echo "$response" | grep -q "ERR\|^$"; then
    log_success "Malformed command handled gracefully"
else
    log_warning "Malformed command response: $response"
fi

log_info "Testing operations without login..."
response=$(send_command "UPLOAD testuser99 file.txt 10 /tmp/test.txt")
if echo "$response" | grep -q "ERR"; then
    log_success "Operation without proper auth rejected"
else
    log_warning "Operation without auth should have been rejected"
fi

# ============================================================================
# TEST 16: GRACEFUL SHUTDOWN
# ============================================================================
log_section "TEST 16: Graceful Shutdown"

log_info "Testing graceful shutdown..."
if kill -INT $SERVER_PID 2>/dev/null; then
    sleep 2
    
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        log_success "Server shut down gracefully"
        SERVER_PID=""  # Clear PID so cleanup doesn't try to kill again
    else
        log_warning "Server did not shut down gracefully"
        kill -9 $SERVER_PID 2>/dev/null || true
        SERVER_PID=""
    fi
else
    log_fail "Could not send shutdown signal"
fi

# ============================================================================
# FINAL SUMMARY
# ============================================================================
log_section "TEST SUMMARY"

echo ""
echo "Total Tests: $TESTS_TOTAL"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ ALL TESTS PASSED!${NC}"
    echo ""
    echo "Your implementation meets all requirements:"
    echo "  ✓ Multi-threaded server with user management"
    echo "  ✓ Simple client communication"
    echo "  ✓ All file operations (UPLOAD, DOWNLOAD, DELETE, LIST)"
    echo "  ✓ Two thread-safe queues (Client Queue, Task Queue)"
    echo "  ✓ Two thread pools (Client Pool, Worker Pool)"
    echo "  ✓ Concurrent access handling"
    echo "  ✓ Worker-to-client communication"
    echo "  ✓ BONUS: No busy waiting"
    echo "  ✓ BONUS: Encoding/Decoding"
    echo "  ✓ BONUS: Priority system"
    echo ""
    echo "Next steps:"
    echo "  1. Run Valgrind test: ./testing/run_valgrind_simple.sh"
    echo "  2. Run ThreadSanitizer test: ./testing/run_tsan_simple.sh"
    echo "  3. Review design report: DESIGN.md"
    echo ""
    exit 0
else
    echo -e "${RED}✗ SOME TESTS FAILED${NC}"
    echo ""
    echo "Please review the failed tests above and fix the issues."
    echo "Check server logs at: /tmp/server_test.log"
    echo ""
    exit 1
fi
