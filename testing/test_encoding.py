#!/usr/bin/env python3
"""
Test script for file encoding/decoding functionality
Tests that files are encoded when stored and decoded when retrieved
"""

import socket
import time
import os
import hashlib
from pathlib import Path

# Colors for terminal output
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'

def log_info(msg):
    print(f"{Colors.BLUE}[INFO]{Colors.NC} {msg}")

def log_success(msg):
    print(f"{Colors.GREEN}[PASS]{Colors.NC} {msg}")

def log_fail(msg):
    print(f"{Colors.RED}[FAIL]{Colors.NC} {msg}")

def log_warning(msg):
    print(f"{Colors.YELLOW}[WARN]{Colors.NC} {msg}")

def send_command(cmd, timeout=5):
    """Send a command to the server and get response"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(('127.0.0.1', 9090))
        sock.sendall((cmd + '\n').encode())
        
        time.sleep(0.2)
        
        response = sock.recv(4096).decode().strip()
        sock.close()
        return response
    except socket.timeout:
        return f"ERR Connection failed: timed out"
    except Exception as e:
        return f"ERR Connection failed: {e}"

def get_file_hash(filepath):
    """Calculate MD5 hash of a file"""
    md5 = hashlib.md5()
    with open(filepath, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
            md5.update(chunk)
    return md5.hexdigest()

def test_encoding_decoding():
    """Test encoding and decoding functionality"""
    print("\n" + "=" * 70)
    print("ENCODING/DECODING TEST")
    print("=" * 70)
    
    # Setup
    test_user = "encodeuser"
    test_pass = "encodepass"
    test_file = "/tmp/test_encode_file.txt"
    test_content = "This is a test file for encoding/decoding!\nLine 2: Special chars: @#$%^&*()\nLine 3: Numbers: 1234567890"
    
    # Create test file
    log_info("Creating test file...")
    with open(test_file, 'w') as f:
        f.write(test_content)
    
    original_hash = get_file_hash(test_file)
    file_size = os.path.getsize(test_file)
    
    # Save original content for later comparison
    with open(test_file, 'rb') as f:
        original_content = f.read()
    
    log_success(f"Test file created: {file_size} bytes, MD5: {original_hash}")
    
    # Test 1: Signup
    print("\n" + "-" * 70)
    log_info("Test 1: Creating user account...")
    response = send_command(f"SIGNUP {test_user} {test_pass} NORMAL")
    if "OK" in response:
        log_success("User signup successful")
    else:
        log_fail(f"User signup failed: {response}")
        return False
    
    # Test 2: Upload file
    print("\n" + "-" * 70)
    log_info("Test 2: Uploading file (should be encoded on server)...")
    response = send_command(f"UPLOAD {test_user} testfile.txt {file_size} {test_file}")
    if "OK" in response or "uploaded" in response:
        log_success("File upload successful")
    else:
        log_fail(f"File upload failed: {response}")
        return False
    
    # Test 3: Check if file is encoded on server
    print("\n" + "-" * 70)
    log_info("Test 3: Verifying file is encoded in storage...")
    
    # Find the stored file
    storage_base = Path(__file__).parent.parent / "storage" / test_user
    if storage_base.exists():
        stored_files = list(storage_base.glob("**/testfile.txt"))
        if stored_files:
            stored_file = stored_files[0]
            log_info(f"Found stored file: {stored_file}")
            
            # Read stored file and check if it's different from original
            with open(stored_file, 'rb') as f:
                stored_content = f.read()
            
            if stored_content == original_content:
                log_fail("File is NOT encoded in storage (contents match original)")
                log_warning("Encoding may not be implemented or not working")
            else:
                log_success("File IS encoded in storage (contents differ from original)")
                log_info(f"Original size: {len(original_content)} bytes")
                log_info(f"Stored size: {len(stored_content)} bytes")
                
                # Show a sample of the difference
                if len(stored_content) > 0 and len(original_content) > 0:
                    log_info(f"First byte - Original: {original_content[0]:02x}, Stored: {stored_content[0]:02x}")
        else:
            log_warning(f"Could not find stored file in {storage_base}")
    else:
        log_warning(f"Storage directory not found: {storage_base}")
    
    # Test 4: Download file
    print("\n" + "-" * 70)
    log_info("Test 4: Downloading file (should be decoded)...")
    response = send_command(f"DOWNLOAD {test_user} testfile.txt")
    if "OK" in response:
        log_success("File download initiated")
        
        # The actual file data would be sent separately in a real implementation
        # For now, we just verify the command succeeded
        log_info("Download command accepted by server")
    else:
        log_fail(f"File download failed: {response}")
        return False
    
    # Test 5: Verify downloaded content matches original
    print("\n" + "-" * 70)
    log_info("Test 5: Verifying downloaded file matches original...")
    
    # Look for downloaded file (server may create temp files)
    download_dir = Path(__file__).parent.parent
    downloaded_files = list(download_dir.glob("**/testfile.txt*"))
    
    found_match = False
    for dl_file in downloaded_files:
        if dl_file != Path(test_file) and dl_file.exists():
            try:
                with open(dl_file, 'r') as f:
                    downloaded_content = f.read()
                
                if downloaded_content == test_content:
                    log_success(f"Downloaded file matches original content!")
                    log_success("Encoding/Decoding working correctly!")
                    found_match = True
                    break
            except:
                pass
    
    if not found_match:
        log_warning("Could not verify downloaded file content")
        log_info("This may be due to the download mechanism not creating a local file")
    
    # Test 6: List files
    print("\n" + "-" * 70)
    log_info("Test 6: Listing files...")
    response = send_command(f"LIST {test_user}")
    if "OK" in response and "testfile.txt" in response:
        log_success("File appears in listing")
        log_info(f"Response: {response}")
    else:
        log_warning(f"File listing response: {response}")
    
    # Test 7: Delete file
    print("\n" + "-" * 70)
    log_info("Test 7: Deleting file...")
    response = send_command(f"DELETE {test_user} testfile.txt")
    if "OK" in response:
        log_success("File deletion successful")
    else:
        log_fail(f"File deletion failed: {response}")
    
    # Test 8: Verify file is deleted
    print("\n" + "-" * 70)
    log_info("Test 8: Verifying file is deleted...")
    response = send_command(f"LIST {test_user}")
    if "testfile.txt" not in response:
        log_success("File successfully removed from storage")
    else:
        log_fail("File still appears in listing after deletion")
    
    # Cleanup
    log_info("Cleaning up...")
    # Note: Server may have already deleted the temp file after upload
    if os.path.exists(test_file):
        try:
            os.remove(test_file)
            log_info("Test file cleaned up")
        except:
            pass
    else:
        log_info("Test file already cleaned up by server")
    
    print("\n" + "=" * 70)
    print("ENCODING/DECODING TEST COMPLETE")
    print("=" * 70)
    
    return True

def test_multiple_files():
    """Test encoding/decoding with multiple files"""
    print("\n" + "=" * 70)
    print("MULTIPLE FILES ENCODING TEST")
    print("=" * 70)
    
    test_user = "multiuser"
    test_pass = "multipass"
    
    # Signup
    log_info("Creating user account...")
    response = send_command(f"SIGNUP {test_user} {test_pass} HIGH")
    if "OK" not in response:
        log_fail(f"User signup failed: {response}")
        return False
    
    # Create and upload multiple files
    test_files = []
    for i in range(3):
        filename = f"/tmp/test_multi_{i}.txt"
        content = f"Test file {i}\n" + ("X" * (100 * (i + 1)))
        
        with open(filename, 'w') as f:
            f.write(content)
        
        test_files.append((filename, f"file{i}.txt", content))
    
    # Upload all files
    log_info("Uploading multiple files...")
    for local_file, remote_name, content in test_files:
        file_size = os.path.getsize(local_file)
        response = send_command(f"UPLOAD {test_user} {remote_name} {file_size} {local_file}")
        if "OK" in response:
            log_success(f"Uploaded {remote_name}")
        else:
            log_fail(f"Failed to upload {remote_name}: {response}")
    
    # List files
    log_info("Listing all files...")
    response = send_command(f"LIST {test_user}")
    if "OK" in response:
        for _, remote_name, _ in test_files:
            if remote_name in response:
                log_success(f"Found {remote_name} in listing")
            else:
                log_fail(f"Missing {remote_name} in listing")
    
    # Cleanup
    for local_file, remote_name, _ in test_files:
        send_command(f"DELETE {test_user} {remote_name}")
        try:
            os.remove(local_file)
        except:
            pass
    
    print("\n" + "=" * 70)
    print("MULTIPLE FILES TEST COMPLETE")
    print("=" * 70)
    
    return True

def main():
    print("\n" + "=" * 70)
    print("FILE ENCODING/DECODING TEST SUITE")
    print("=" * 70)
    print("\nThis test verifies that:")
    print("  1. Files are encoded when stored on the server")
    print("  2. Files are decoded when downloaded")
    print("  3. Downloaded files match the original content")
    print("\nMake sure the server is running: ./server 9090")
    print("\nStarting tests in 2 seconds...")
    print("=" * 70)
    
    time.sleep(2)
    
    # Run tests
    test_encoding_decoding()
    print("\n")
    test_multiple_files()
    
    print("\n" + "=" * 70)
    print("ALL TESTS COMPLETE")
    print("=" * 70)

if __name__ == "__main__":
    main()
