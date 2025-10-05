#!/bin/bash

# Test script for the file storage client-server system
# This script demonstrates basic functionality

echo "=== File Storage Server Test ==="
echo

# Check if server and client are built
if [ ! -f "./server" ]; then
    echo "Error: Server not found. Run 'make' first."
    exit 1
fi

if [ ! -f "./client" ]; then
    echo "Error: Client not found. Run 'make' first."
    exit 1
fi

# Create test files
echo "Creating test files..."
echo "Hello World!" > test_file1.txt
echo "This is a test file for upload." > test_file2.txt
echo "Test files created."

echo
echo "Starting server in background..."
./server 9090 &
SERVER_PID=$!

# Wait for server to start
sleep 2

echo "Server started with PID: $SERVER_PID"
echo

# Test client commands
echo "Testing client commands..."

# Test 1: Signup
echo "Test 1: User signup"
echo "signup testuser password123" | ./client 127.0.0.1 9090
echo

# Test 2: Login
echo "Test 2: User login"
echo "login testuser password123" | ./client 127.0.0.1 9090
echo

# Test 3: Upload file
echo "Test 3: File upload"
echo -e "login testuser password123\nupload test_file1.txt\nquit" | ./client 127.0.0.1 9090
echo

# Test 4: List files
echo "Test 4: List files"
echo -e "login testuser password123\nlist\nquit" | ./client 127.0.0.1 9090
echo

# Test 5: Download file
echo "Test 5: File download"
echo -e "login testuser password123\ndownload test_file1.txt\nquit" | ./client 127.0.0.1 9090
echo

# Test 6: Delete file
echo "Test 6: File delete"
echo -e "login testuser password123\ndelete test_file1.txt\nquit" | ./client 127.0.0.1 9090
echo

# Cleanup
echo "Cleaning up..."
kill $SERVER_PID 2>/dev/null
rm -f test_file1.txt test_file2.txt

echo "Test completed!"
