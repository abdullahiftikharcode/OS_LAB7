#!/usr/bin/env python3
"""
Simple test client for ThreadSanitizer testing.
Sends one command and waits for response properly.
"""

import socket
import sys
import time

def send_command(host, port, command):
    """Send a single command and get response."""
    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)  # Increased timeout
        sock.connect((host, port))
        
        # Send command with newline
        sock.sendall((command + '\n').encode())
        
        # Receive response (blocking until we get data)
        response = sock.recv(1024).decode().strip()
        
        # Properly close the socket
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except:
            pass
        sock.close()
        return response
    except socket.timeout:
        return f"TIMEOUT: No response received"
    except Exception as e:
        return f"ERROR: {e}"
    finally:
        if sock:
            try:
                sock.close()
            except:
                pass

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: tsan_test_client.py <command>")
        sys.exit(1)
    
    command = sys.argv[1]
    host = "localhost"
    port = 9090
    
    result = send_command(host, port, command)
    print(result)
    sys.stdout.flush()
    sys.exit(0)
