# File Storage Client

This is a client program for the multi-threaded file storage server. It provides an interactive command-line interface to communicate with the server.

## Building

To build both the server and client:

```bash
make
```

This will create two executables:
- `server` - The file storage server
- `client` - The client program

## Usage

### Starting the Server

```bash
./server [port]
```

Default port is 9090. Example:
```bash
./server 9090
```

### Running the Client

```bash
./client [host] [port]
```

Default host is 127.0.0.1 and default port is 9090. Examples:
```bash
./client                    # Connect to localhost:9090
./client 192.168.1.100      # Connect to 192.168.1.100:9090
./client 192.168.1.100 8080 # Connect to 192.168.1.100:8080
```

## Available Commands

Once connected, you can use the following commands:

### User Management
- `signup <username> <password>` - Create a new user account
- `login <username> <password>` - Login to an existing account

### File Operations
- `upload <filename>` - Upload a file to the server
- `download <filename>` - Download a file from the server
- `delete <filename>` - Delete a file from the server
- `list` - List all files for the current user

### Other
- `quit` - Disconnect from the server

## Example Session

```
$ ./client
Connecting to server at 127.0.0.1:9090

=== File Storage Client ===
Available commands:
  signup <username> <password>  - Create new account
  login <username> <password>   - Login to account
  upload <filename>            - Upload file
  download <filename>          - Download file
  delete <filename>            - Delete file
  list                         - List files
  quit                         - Exit

> signup alice mypassword
Signup response: OK User created successfully

> login alice mypassword
Login response: OK Login successful

> upload myfile.txt
Upload response: OK File uploaded successfully

> list
List response: OK Files: myfile.txt

> download myfile.txt
Download response: OK File downloaded successfully

> delete myfile.txt
Delete response: OK File deleted successfully

> quit
Client disconnected
```

## Protocol Details

The client communicates with the server using a simple text-based protocol:

### Commands Sent to Server:
- `SIGNUP <username> <password>\n`
- `LOGIN <username> <password>\n`
- `UPLOAD <username> <filename> <size> <tmpfile>\n`
- `DOWNLOAD <username> <filename>\n`
- `DELETE <username> <filename>\n`
- `LIST <username>\n`
- `QUIT\n`

### Responses from Server:
- `OK <message>\n` - Success
- `ERR <message>\n` - Error

## Testing

A test script is provided to demonstrate the client-server interaction:

```bash
./test_client.sh
```

This script will:
1. Start the server in the background
2. Test various client commands
3. Clean up test files and stop the server

## Features

- **Interactive Mode**: Command-line interface with helpful prompts
- **Error Handling**: Proper error messages for common issues
- **File Operations**: Complete CRUD operations for files
- **User Authentication**: Signup and login functionality
- **Connection Management**: Automatic connection cleanup
- **Cross-platform**: Works on Linux, macOS, and Windows (with WSL)

## Notes

- The client creates temporary files during upload operations
- File data is copied to temporary locations before being sent to the server
- The client handles server disconnections gracefully
- All commands require authentication except signup and login
