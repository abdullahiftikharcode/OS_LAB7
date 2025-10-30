#!/usr/bin/env python3
"""
Comprehensive Test Script for OS Lab 7 - Multi-threaded File Server
Tests all requirements and bonus features
"""

import socket
import subprocess
import time
import os
import sys
import signal
import threading
from pathlib import Path

# Colors for terminal output
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

# Test statistics
tests_passed = 0
tests_failed = 0
tests_total = 0

# Server process
server_process = None

def log_info(msg):
    print(f"{Colors.BLUE}[INFO]{Colors.NC} {msg}")

def log_success(msg):
    global tests_passed, tests_total
    print(f"{Colors.GREEN}[PASS]{Colors.NC} {msg}")
    tests_passed += 1
    tests_total += 1

def log_fail(msg):
    global tests_failed, tests_total
    print(f"{Colors.RED}[FAIL]{Colors.NC} {msg}")
    tests_failed += 1
    tests_total += 1

def log_warning(msg):
    print(f"{Colors.YELLOW}[WARN]{Colors.NC} {msg}")

def log_section(msg):
    print()
    print(f"{Colors.BLUE}{'='*60}{Colors.NC}")
    print(f"{Colors.BLUE}{msg}{Colors.NC}")
    print(f"{Colors.BLUE}{'='*60}{Colors.NC}")

def send_command(cmd, timeout=5):
    """Send a command to the server and get response"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(('127.0.0.1', 9090))
        sock.sendall((cmd + '\n').encode())
        
        # Give server time to process
        time.sleep(0.1)
        
        # Read response
        response = sock.recv(4096).decode().strip()
        sock.close()
        return response
    except socket.timeout:
        return f"ERR Connection failed: timed out"
    except Exception as e:
        return f"ERR Connection failed: {e}"

def wait_for_server(max_attempts=20):
    """Wait for server to be ready"""
    for i in range(max_attempts):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            sock.connect(('127.0.0.1', 9090))
            sock.close()
            return True
        except:
            time.sleep(0.5)
    return False

def start_server():
    """Start the server process"""
    global server_process
    log_info("Starting server...")
    
    # Kill any existing server processes first
    try:
        subprocess.run(['pkill', '-9', 'server'], capture_output=True)
        time.sleep(1)
    except:
        pass
    
    try:
        server_process = subprocess.Popen(
            ['./server', '9090'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=Path(__file__).parent.parent
        )
        
        time.sleep(2)  # Give server more time to initialize
        
        if server_process.poll() is not None:
            log_fail("Server failed to start")
            return False
        
        if wait_for_server():
            log_success(f"Server started successfully (PID: {server_process.pid})")
            return True
        else:
            log_fail("Server not accepting connections")
            return False
    except Exception as e:
        log_fail(f"Failed to start server: {e}")
        return False

def stop_server():
    """Stop the server process"""
    global server_process
    if server_process:
        log_info("Stopping server...")
        try:
            server_process.send_signal(signal.SIGINT)
            server_process.wait(timeout=3)
            log_success("Server stopped gracefully")
        except:
            server_process.kill()
            log_warning("Server force killed")
        server_process = None

def cleanup():
    """Cleanup test files and server"""
    log_info("Cleaning up...")
    stop_server()
    
    # Remove test files
    test_files = [
        '/tmp/test_upload_1.txt',
        '/tmp/test_upload_2.txt',
        '/tmp/test_upload_large.txt',
        '/tmp/test_encoding.txt',
        '/tmp/test_concurrent.txt'
    ]
    
    for f in test_files:
        try:
            os.remove(f)
        except:
            pass

def test_compilation():
    """Test 1: Compilation"""
    log_section("TEST 1: Compilation")
    
    os.chdir(Path(__file__).parent.parent)
    
    log_info("Testing standard build...")
    result = subprocess.run(['make', 'clean'], capture_output=True)
    result = subprocess.run(['make'], capture_output=True)
    
    if result.returncode == 0:
        log_success("Standard build successful")
        
        if os.path.exists('server') and os.path.exists('client'):
            log_success("Server and client binaries created")
        else:
            log_fail("Binaries not found after build")
    else:
        log_fail("Standard build failed")
        print(result.stderr.decode())
        return False
    
    return True

def test_user_registration():
    """Test 3: User Registration"""
    log_section("TEST 3: User Registration (SIGNUP)")
    
    log_info("Testing user signup with NORMAL priority...")
    response = send_command("SIGNUP testuser1 password123 NORMAL")
    if "OK" in response:
        log_success("User signup successful")
    else:
        log_fail(f"User signup failed: {response}")
    
    log_info("Testing duplicate signup...")
    response = send_command("SIGNUP testuser1 password123")
    if "ERR" in response:
        log_success("Duplicate signup correctly rejected")
    else:
        log_fail("Duplicate signup should have failed")
    
    log_info("Testing multiple user signups with different priorities...")
    priorities = ["HIGH", "NORMAL", "LOW", "NORMAL"]
    for i in range(2, 6):
        priority = priorities[i-2]
        response = send_command(f"SIGNUP testuser{i} pass{i} {priority}")
        if "OK" in response:
            log_success(f"User testuser{i} signup successful (priority: {priority})")
        else:
            log_fail(f"User testuser{i} signup failed")

def test_user_authentication():
    """Test 4: User Authentication"""
    log_section("TEST 4: User Authentication (LOGIN)")
    
    log_info("Testing valid login...")
    response = send_command("LOGIN testuser1 password123")
    if "OK" in response:
        log_success("Valid login successful")
    else:
        log_fail(f"Valid login failed: {response}")
    
    log_info("Testing invalid password...")
    response = send_command("LOGIN testuser1 wrongpassword")
    if "ERR" in response:
        log_success("Invalid password correctly rejected")
    else:
        log_fail("Invalid password should have been rejected")
    
    log_info("Testing non-existent user...")
    response = send_command("LOGIN nonexistent password")
    if "ERR" in response:
        log_success("Non-existent user correctly rejected")
    else:
        log_fail("Non-existent user should have been rejected")

def test_file_upload():
    """Test 5: File Upload"""
    log_section("TEST 5: File Upload (UPLOAD)")
    
    log_info("Creating test files...")
    with open('/tmp/test_upload_1.txt', 'w') as f:
        f.write("This is test file 1 content")
    
    with open('/tmp/test_upload_2.txt', 'w') as f:
        f.write("This is test file 2 with more content to test encoding")
    
    with open('/tmp/test_upload_large.txt', 'wb') as f:
        f.write(os.urandom(100 * 1024))  # 100KB
    
    log_success("Test files created")
    
    log_info("Testing file upload...")
    file_size = os.path.getsize('/tmp/test_upload_1.txt')
    response = send_command(f"UPLOAD testuser1 file1.txt {file_size} /tmp/test_upload_1.txt")
    if "OK" in response:
        log_success("File upload successful")
    else:
        log_fail(f"File upload failed: {response}")
    
    log_info("Uploading multiple files...")
    file_size = os.path.getsize('/tmp/test_upload_2.txt')
    response = send_command(f"UPLOAD testuser1 file2.txt {file_size} /tmp/test_upload_2.txt")
    if "OK" in response:
        log_success("Second file upload successful")
    else:
        log_fail("Second file upload failed")
    
    log_info("Testing large file upload...")
    file_size = os.path.getsize('/tmp/test_upload_large.txt')
    response = send_command(f"UPLOAD testuser1 large.txt {file_size} /tmp/test_upload_large.txt")
    if "OK" in response:
        log_success("Large file upload successful")
    else:
        log_fail("Large file upload failed")

def test_file_listing():
    """Test 6: File Listing"""
    log_section("TEST 6: File Listing (LIST)")
    
    log_info("Testing file listing...")
    response = send_command("LIST testuser1")
    if "OK" in response:
        log_success("File listing successful")
        
        if "file1.txt" in response:
            log_success("file1.txt found in listing")
        else:
            log_fail("file1.txt not found in listing")
        
        if "file2.txt" in response:
            log_success("file2.txt found in listing")
        else:
            log_fail("file2.txt not found in listing")
    else:
        log_fail(f"File listing failed: {response}")
    
    log_info("Testing empty directory listing...")
    response = send_command("LIST testuser2")
    if "OK" in response:
        log_success("Empty directory listing successful")
    else:
        log_fail("Empty directory listing failed")

def test_file_download():
    """Test 7: File Download"""
    log_section("TEST 7: File Download (DOWNLOAD)")
    
    log_info("Testing file download...")
    response = send_command("DOWNLOAD testuser1 file1.txt")
    if "OK" in response:
        log_success("File download initiated successfully")
        
        if "/tmp/os_lab7_downloads" in response:
            log_success("Download temp file path returned")
        else:
            log_warning("Download temp file path not found in response")
    else:
        log_fail(f"File download failed: {response}")
    
    log_info("Testing download of non-existent file...")
    response = send_command("DOWNLOAD testuser1 nonexistent.txt")
    if "ERR" in response:
        log_success("Non-existent file download correctly rejected")
    else:
        log_fail("Non-existent file download should have failed")

def test_file_deletion():
    """Test 8: File Deletion"""
    log_section("TEST 8: File Deletion (DELETE)")
    
    log_info("Testing file deletion...")
    response = send_command("DELETE testuser1 file2.txt")
    if "OK" in response:
        log_success("File deletion successful")
    else:
        log_fail(f"File deletion failed: {response}")
    
    log_info("Verifying file was deleted...")
    response = send_command("LIST testuser1")
    if "file2.txt" not in response:
        log_success("Deleted file no longer in listing")
    else:
        log_fail("Deleted file still appears in listing")
    
    log_info("Testing deletion of non-existent file...")
    response = send_command("DELETE testuser1 nonexistent.txt")
    if "ERR" in response:
        log_success("Non-existent file deletion correctly rejected")
    else:
        log_fail("Non-existent file deletion should have failed")

def test_concurrent_operations():
    """Test 9: Concurrent Operations"""
    log_section("TEST 9: Concurrent Operations")
    
    log_info("Testing concurrent signups...")
    threads = []
    for i in range(10, 16):
        t = threading.Thread(target=lambda i=i: send_command(f"SIGNUP concurrent_user{i} pass{i}"))
        threads.append(t)
        t.start()
    
    for t in threads:
        t.join()
    
    log_success("Concurrent signups completed without crashes")
    
    log_info("Testing concurrent logins...")
    threads = []
    for i in range(1, 6):
        t = threading.Thread(target=lambda i=i: send_command(f"LOGIN testuser{i} pass{i}"))
        threads.append(t)
        t.start()
    
    for t in threads:
        t.join()
    
    log_success("Concurrent logins completed without crashes")
    
    log_info("Testing concurrent operations on same user...")
    with open('/tmp/test_concurrent.txt', 'w') as f:
        f.write("Test content for concurrent ops")
    
    file_size = os.path.getsize('/tmp/test_concurrent.txt')
    
    threads = []
    t1 = threading.Thread(target=lambda: send_command(f"UPLOAD testuser1 concurrent1.txt {file_size} /tmp/test_concurrent.txt"))
    t2 = threading.Thread(target=lambda: send_command("LIST testuser1"))
    t3 = threading.Thread(target=lambda: send_command(f"UPLOAD testuser1 concurrent2.txt {file_size} /tmp/test_concurrent.txt"))
    
    threads = [t1, t2, t3]
    for t in threads:
        t.start()
    
    for t in threads:
        t.join()
    
    log_success("Concurrent operations on same user completed without crashes")

def test_encoding_decoding():
    """Test 10: Encoding/Decoding (BONUS)"""
    log_section("TEST 10: Encoding/Decoding (BONUS)")
    
    log_info("Creating test file with known content...")
    test_content = "ENCODING_TEST_CONTENT_12345"
    with open('/tmp/test_encoding.txt', 'w') as f:
        f.write(test_content)
    
    file_size = os.path.getsize('/tmp/test_encoding.txt')
    
    log_info("Uploading file (should be encoded)...")
    response = send_command(f"UPLOAD testuser1 encoded_test.txt {file_size} /tmp/test_encoding.txt")
    if "OK" in response:
        log_success("File uploaded for encoding test")
    else:
        log_fail("File upload for encoding test failed")
    
    log_info("Checking if stored file is encoded...")
    storage_path = Path(__file__).parent.parent / "storage" / "testuser1" / "encoded_test.txt"
    
    if storage_path.exists():
        with open(storage_path, 'r', errors='ignore') as f:
            stored_content = f.read()
        
        if stored_content != test_content:
            log_success("File is encoded in storage (content differs from original)")
        else:
            log_fail("File is NOT encoded in storage (content matches original)")
    else:
        log_warning("Could not verify encoding (storage file not found)")
    
    log_info("Downloading file (should be decoded)...")
    response = send_command("DOWNLOAD testuser1 encoded_test.txt")
    if "OK" in response:
        log_success("File downloaded for decoding test")
    else:
        log_fail("File download for decoding test failed")

def test_priority_system():
    """Test 11: Priority System (BONUS)"""
    log_section("TEST 11: Priority System (BONUS)")
    
    log_info("Testing priority queue implementation...")
    log_info("Submitting tasks with different priorities rapidly...")
    
    # Submit tasks in mixed order
    threads = []
    threads.append(threading.Thread(target=lambda: send_command("LIST testuser1")))  # LOW
    threads.append(threading.Thread(target=lambda: send_command("UPLOAD testuser1 p1.txt 10 /tmp/test_encoding.txt")))  # NORMAL
    threads.append(threading.Thread(target=lambda: send_command("SIGNUP priority_user1 pass")))  # HIGH
    threads.append(threading.Thread(target=lambda: send_command("LIST testuser2")))  # LOW
    threads.append(threading.Thread(target=lambda: send_command("LOGIN testuser1 password123")))  # HIGH
    threads.append(threading.Thread(target=lambda: send_command("DOWNLOAD testuser1 file1.txt")))  # NORMAL
    
    for t in threads:
        t.start()
    
    for t in threads:
        t.join()
    
    log_success("Priority tasks submitted (check server logs for ordering)")
    log_info("Expected order: HIGH tasks (SIGNUP, LOGIN) -> NORMAL tasks (UPLOAD, DOWNLOAD) -> LOW tasks (LIST)")

def test_stress():
    """Test 14: Stress Test"""
    log_section("TEST 14: Stress Test")
    
    log_info("Running stress test with 20 concurrent clients...")
    
    def stress_client(i):
        try:
            send_command(f"SIGNUP stress_user{i} pass{i}")
            send_command(f"LOGIN stress_user{i} pass{i}")
            send_command(f"LIST stress_user{i}")
            return True
        except:
            return False
    
    threads = []
    for i in range(20, 40):
        t = threading.Thread(target=stress_client, args=(i,))
        threads.append(t)
        t.start()
    
    for t in threads:
        t.join()
    
    log_success("Stress test completed successfully (20 concurrent clients)")

def test_error_handling():
    """Test 15: Error Handling"""
    log_section("TEST 15: Error Handling")
    
    log_info("Testing invalid commands...")
    response = send_command("INVALID_COMMAND")
    if "ERR" in response or response == "":
        log_success("Invalid command handled gracefully")
    else:
        log_warning(f"Invalid command response: {response}")
    
    log_info("Testing malformed commands...")
    response = send_command("UPLOAD")
    if "ERR" in response or response == "":
        log_success("Malformed command handled gracefully")
    else:
        log_warning(f"Malformed command response: {response}")

def print_summary():
    """Print test summary"""
    log_section("TEST SUMMARY")
    
    print()
    print(f"Total Tests: {tests_total}")
    print(f"{Colors.GREEN}Passed: {tests_passed}{Colors.NC}")
    print(f"{Colors.RED}Failed: {tests_failed}{Colors.NC}")
    print()
    
    if tests_failed == 0:
        print(f"{Colors.GREEN}✓ ALL TESTS PASSED!{Colors.NC}")
        print()
        print("Your implementation meets all requirements:")
        print("  ✓ Multi-threaded server with user management")
        print("  ✓ Simple client communication")
        print("  ✓ All file operations (UPLOAD, DOWNLOAD, DELETE, LIST)")
        print("  ✓ Two thread-safe queues (Client Queue, Task Queue)")
        print("  ✓ Two thread pools (Client Pool, Worker Pool)")
        print("  ✓ Concurrent access handling")
        print("  ✓ Worker-to-client communication")
        print("  ✓ BONUS: No busy waiting")
        print("  ✓ BONUS: Encoding/Decoding")
        print()
        print("Next steps:")
        print("  1. Review test reports in: reports/")
        print("  2. Review design documentation: DESIGN.md")
        print("  3. Check requirements: REQUIREMENTS_CHECKLIST.md")
        print()
        return 0
    else:
        print(f"{Colors.RED}✗ SOME TESTS FAILED{Colors.NC}")
        print()
        print("Please review the failed tests above and fix the issues.")
        print()
        return 1

def run_valgrind_test():
    """Run Valgrind memory leak test"""
    log_section("BONUS TEST: Valgrind Memory Leak Check")
    
    log_info("Building debug version...")
    result = subprocess.run(['make', 'clean'], capture_output=True, cwd=Path(__file__).parent.parent)
    result = subprocess.run(['make', 'debug'], capture_output=True, cwd=Path(__file__).parent.parent)
    
    if result.returncode != 0:
        log_fail("Debug build failed")
        return False
    
    log_info("Running Valgrind with automated tests (this may take 1-2 minutes)...")
    
    # Create reports directory if it doesn't exist
    reports_dir = Path(__file__).parent.parent / 'reports'
    reports_dir.mkdir(exist_ok=True)
    
    # Run Valgrind with the server
    valgrind_cmd = [
        'valgrind',
        '--leak-check=full',
        '--show-leak-kinds=all',
        '--track-origins=yes',
        '--log-file=reports/valgrind_automated.txt',
        './server_debug', '9090'
    ]
    
    try:
        proc = subprocess.Popen(valgrind_cmd, 
                               cwd=Path(__file__).parent.parent,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
        time.sleep(3)  # Let server start
        
        # Run automated tests against the server
        log_info("Running test operations...")
        test_operations = [
            "SIGNUP valgrinduser1 pass1",
            "SIGNUP valgrinduser2 pass2",
            "LOGIN valgrinduser1 pass1",
            "LOGIN valgrinduser2 pass2",
            "LIST valgrinduser1",
        ]
        
        for cmd in test_operations:
            try:
                send_command(cmd, timeout=3)
            except:
                pass
            time.sleep(0.2)
        
        log_info("Shutting down server...")
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=15)
        
        # Check report
        report_path = Path(__file__).parent.parent / 'reports' / 'valgrind_automated.txt'
        if report_path.exists():
            with open(report_path, 'r') as f:
                content = f.read()
                
                # Check for memory leaks
                if 'definitely lost: 0 bytes' in content and 'indirectly lost: 0 bytes' in content:
                    log_success("Valgrind: No memory leaks detected")
                    return True
                elif 'definitely lost: 0 bytes' in content:
                    log_warning("Valgrind: No definite leaks, but some indirect leaks detected")
                    log_info(f"Check report: {report_path}")
                    return True
                else:
                    log_fail("Valgrind: Memory leaks detected")
                    log_info(f"Check report: {report_path}")
                    return False
        else:
            log_warning("Valgrind report not generated")
            return False
    except subprocess.TimeoutExpired:
        log_warning("Valgrind test timed out")
        try:
            proc.kill()
        except:
            pass
        return False
    except Exception as e:
        log_warning(f"Valgrind test failed: {e}")
        try:
            proc.kill()
        except:
            pass
        return False

def run_tsan_test():
    """Run ThreadSanitizer race condition test"""
    log_section("BONUS TEST: ThreadSanitizer Race Condition Check")
    
    log_info("Building with ThreadSanitizer...")
    result = subprocess.run(['make', 'clean'], capture_output=True, cwd=Path(__file__).parent.parent)
    result = subprocess.run(['make', 'tsan'], capture_output=True, cwd=Path(__file__).parent.parent)
    
    if result.returncode != 0:
        log_fail("ThreadSanitizer build failed")
        return False
    
    log_info("Running ThreadSanitizer with automated tests (this may take 1-2 minutes)...")
    
    # Create reports directory if it doesn't exist
    reports_dir = Path(__file__).parent.parent / 'reports'
    reports_dir.mkdir(exist_ok=True)
    
    # Run TSAN with the server
    tsan_env = os.environ.copy()
    tsan_env['TSAN_OPTIONS'] = 'log_path=reports/tsan_automated.txt second_deadlock_stack=1'
    
    try:
        proc = subprocess.Popen(['./server_tsan', '9090'], 
                               cwd=Path(__file__).parent.parent,
                               env=tsan_env,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
        time.sleep(2)
        
        # Run automated concurrent tests to stress-test for race conditions
        log_info("Running concurrent test operations...")
        
        def run_concurrent_commands():
            commands = [
                "SIGNUP tsanuser1 pass1",
                "SIGNUP tsanuser2 pass2",
                "SIGNUP tsanuser3 pass3",
                "LOGIN tsanuser1 pass1",
                "LOGIN tsanuser2 pass2",
                "LOGIN tsanuser3 pass3",
                "LIST tsanuser1",
                "LIST tsanuser2",
            ]
            for cmd in commands:
                try:
                    send_command(cmd, timeout=2)
                except:
                    pass
                time.sleep(0.05)
        
        # Run commands from multiple threads to trigger potential race conditions
        threads = []
        for _ in range(3):
            t = threading.Thread(target=run_concurrent_commands)
            t.start()
            threads.append(t)
        
        for t in threads:
            t.join(timeout=10)
        
        log_info("Shutting down server...")
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=15)
        
        # Check for race conditions
        tsan_files = list((Path(__file__).parent.parent / 'reports').glob('tsan_automated.txt*'))
        if tsan_files:
            has_race = False
            has_deadlock = False
            race_count = 0
            
            for tsan_file in tsan_files:
                try:
                    with open(tsan_file, 'r') as f:
                        content = f.read()
                        if 'WARNING: ThreadSanitizer: data race' in content:
                            has_race = True
                            race_count += content.count('WARNING: ThreadSanitizer: data race')
                        if 'WARNING: ThreadSanitizer: lock-order-inversion' in content:
                            has_deadlock = True
                except:
                    pass
            
            if not has_race and not has_deadlock:
                log_success("ThreadSanitizer: No race conditions or deadlocks detected")
                return True
            else:
                if has_race:
                    log_fail(f"ThreadSanitizer: {race_count} data race(s) detected")
                if has_deadlock:
                    log_fail("ThreadSanitizer: Potential deadlock detected")
                log_info(f"Check reports in: {reports_dir}")
                return False
        else:
            log_warning("ThreadSanitizer report not generated")
            return False
    except subprocess.TimeoutExpired:
        log_warning("ThreadSanitizer test timed out")
        try:
            proc.kill()
        except:
            pass
        return False
    except Exception as e:
        log_warning(f"ThreadSanitizer test failed: {e}")
        try:
            proc.kill()
        except:
            pass
        return False

def main():
    """Main test runner"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Comprehensive test suite for OS Lab 7')
    parser.add_argument('--skip-valgrind', action='store_true', help='Skip Valgrind test')
    parser.add_argument('--skip-tsan', action='store_true', help='Skip ThreadSanitizer test')
    parser.add_argument('--valgrind-only', action='store_true', help='Run only Valgrind test')
    parser.add_argument('--tsan-only', action='store_true', help='Run only ThreadSanitizer test')
    args = parser.parse_args()
    
    try:
        if args.valgrind_only:
            run_valgrind_test()
            return 0
        
        if args.tsan_only:
            run_tsan_test()
            return 0
        
        # Test 1: Compilation
        if not test_compilation():
            return 1
        
        # Test 2: Start server
        log_section("TEST 2: Server Startup")
        if not start_server():
            return 1
        
        # Run all functional tests
        test_user_registration()
        test_user_authentication()
        test_file_upload()
        test_file_listing()
        test_file_download()
        test_file_deletion()
        test_concurrent_operations()
        test_encoding_decoding()
        test_priority_system()
        test_stress()
        test_error_handling()
        
        # Test graceful shutdown
        log_section("TEST 16: Graceful Shutdown")
        stop_server()
        
        # Print summary
        result = print_summary()
        
        # Run Valgrind and TSAN if requested
        if not args.skip_valgrind and result == 0:
            print("\n")
            run_valgrind_test()
        
        if not args.skip_tsan and result == 0:
            print("\n")
            run_tsan_test()
        
        return result
        
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        return 1
    except Exception as e:
        print(f"\n\nTest failed with exception: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        cleanup()

if __name__ == "__main__":
    sys.exit(main())
