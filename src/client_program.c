#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 4096
#define MAX_FILENAME 256
#define MAX_USERNAME 64
#define MAX_PASSWORD 64

typedef struct {
    int socket_fd;
    char username[MAX_USERNAME];
    int logged_in;
} ClientState;

// Function prototypes
int connect_to_server(const char* host, int port);
int send_command(ClientState* client, const char* command);
int receive_response(ClientState* client, char* response, size_t response_size);
int signup(ClientState* client, const char* username, const char* password);
int login(ClientState* client, const char* username, const char* password);
int upload_file(ClientState* client, const char* filename);
int download_file(ClientState* client, const char* filename);
int receive_file_data(ClientState* client, const char* filename);
int delete_file(ClientState* client, const char* filename);
int list_files(ClientState* client);
void interactive_mode(ClientState* client);
void cleanup_client(ClientState* client);

int connect_to_server(const char* host, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int send_command(ClientState* client, const char* command) {
    if (!client->logged_in && strncmp(command, "SIGNUP", 6) != 0 && strncmp(command, "LOGIN", 5) != 0) {
        printf("Error: You must login first\n");
        return -1;
    }

    size_t len = strlen(command);
    ssize_t sent = write(client->socket_fd, command, len);
    if (sent != (ssize_t)len) {
        perror("write");
        return -1;
    }
    return 0;
}

int receive_response(ClientState* client, char* response, size_t response_size) {
    ssize_t received = read(client->socket_fd, response, response_size - 1);
    if (received <= 0) {
        if (received == 0) {
            printf("Server disconnected\n");
        } else {
            perror("read");
        }
        return -1;
    }
    response[received] = '\0';
    return 0;
}

int signup(ClientState* client, const char* username, const char* password) {
    char command[256];
    snprintf(command, sizeof(command), "SIGNUP %s %s\n", username, password);
    
    if (send_command(client, command) < 0) {
        return -1;
    }

    char response[1024];
    if (receive_response(client, response, sizeof(response)) < 0) {
        return -1;
    }

    printf("Signup response: %s", response);
    
    if (strncmp(response, "OK", 2) == 0) {
        strncpy(client->username, username, sizeof(client->username) - 1);
        client->username[sizeof(client->username) - 1] = '\0';
        client->logged_in = 1;
        return 0;
    }
    return -1;
}

int login(ClientState* client, const char* username, const char* password) {
    char command[256];
    snprintf(command, sizeof(command), "LOGIN %s %s\n", username, password);
    
    if (send_command(client, command) < 0) {
        return -1;
    }

    char response[1024];
    if (receive_response(client, response, sizeof(response)) < 0) {
        return -1;
    }

    printf("Login response: %s", response);
    
    if (strncmp(response, "OK", 2) == 0) {
        strncpy(client->username, username, sizeof(client->username) - 1);
        client->username[sizeof(client->username) - 1] = '\0';
        client->logged_in = 1;
        return 0;
    }
    return -1;
}

int upload_file(ClientState* client, const char* filename) {
    // Check if file exists
    struct stat file_stat;
    if (stat(filename, &file_stat) < 0) {
        printf("Error: File '%s' does not exist\n", filename);
        return -1;
    }

    // Create temporary file for upload
    char tmpfile[256];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/upload_%s_%ld", filename, (long)time(NULL));
    
    // Copy file to temp location (in real implementation, this would be the actual file data)
    int src_fd = open(filename, O_RDONLY);
    if (src_fd < 0) {
        perror("open source file");
        return -1;
    }
    
    int tmp_fd = open(tmpfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (tmp_fd < 0) {
        perror("open temp file");
        close(src_fd);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    size_t total_size = 0;
    
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(tmp_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("write to temp file");
            close(src_fd);
            close(tmp_fd);
            unlink(tmpfile);
            return -1;
        }
        total_size += bytes_written;
    }
    
    close(src_fd);
    close(tmp_fd);

    // Send upload command
    char command[512];
    snprintf(command, sizeof(command), "UPLOAD %s %s %zu %s\n", 
             client->username, filename, total_size, tmpfile);
    
    if (send_command(client, command) < 0) {
        unlink(tmpfile);
        return -1;
    }

    char response[1024];
    if (receive_response(client, response, sizeof(response)) < 0) {
        unlink(tmpfile);
        return -1;
    }

    printf("Upload response: %s", response);
    
    // Clean up temp file
    unlink(tmpfile);
    
    return (strncmp(response, "OK", 2) == 0) ? 0 : -1;
}

int download_file(ClientState* client, const char* filename) {
    char command[512];
    snprintf(command, sizeof(command), "DOWNLOAD %s %s\n", client->username, filename);
    
    if (send_command(client, command) < 0) {
        return -1;
    }

    // For download, we need to handle the response and file data together
    return receive_file_data(client, filename);
}

int receive_file_data(ClientState* client, const char* filename) {
    // First receive the response message
    char response[1024];
    if (receive_response(client, response, sizeof(response)) < 0) {
        return -1;
    }
    
    printf("Download response: %s", response);
    
    if (strncmp(response, "OK", 2) != 0) {
        printf("Download failed: %s", response);
        return -1;
    }
    
    // Now receive the file data header
    char buffer[1024];
    ssize_t bytes_received = recv(client->socket_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        printf("Failed to receive file data header\n");
        return -1;
    }
    
    buffer[bytes_received] = '\0';
    
    // Parse file header: "FILE_DATA filename size\n"
    char received_filename[256];
    long file_size;
    if (sscanf(buffer, "FILE_DATA %255s %ld", received_filename, &file_size) != 2) {
        printf("Invalid file data header: %s\n", buffer);
        return -1;
    }
    
    printf("Receiving file: %s (size: %ld bytes)\n", received_filename, file_size);
    
    // Create output file
    char output_filename[512];
    snprintf(output_filename, sizeof(output_filename), "downloaded_%s", filename);
    int output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        perror("Failed to create output file");
        return -1;
    }
    
    // Calculate remaining data to receive (subtract header size)
    long header_size = strlen(buffer);
    long remaining_bytes = file_size;
    
    // Write any remaining data from the first buffer
    if (bytes_received > header_size) {
        ssize_t data_bytes = bytes_received - header_size;
        write(output_fd, buffer + header_size, data_bytes);
        remaining_bytes -= data_bytes;
    }
    
    // Receive remaining file data
    char data_buffer[8192];
    while (remaining_bytes > 0) {
        size_t buffer_size = sizeof(data_buffer);
        ssize_t to_read = (remaining_bytes > (long)buffer_size) ? buffer_size : (size_t)remaining_bytes;
        
        printf("Waiting to receive %ld more bytes...\n", remaining_bytes);
        ssize_t bytes_read = recv(client->socket_fd, data_buffer, to_read, 0);
        
        if (bytes_read <= 0) {
            printf("Failed to receive file data (bytes_read: %ld)\n", bytes_read);
            close(output_fd);
            return -1;
        }
        
        printf("Received %ld bytes\n", bytes_read);
        write(output_fd, data_buffer, bytes_read);
        remaining_bytes -= bytes_read;
    }
    
    close(output_fd);
    printf("File downloaded successfully as: %s\n", output_filename);
    return 0;
}

int delete_file(ClientState* client, const char* filename) {
    char command[256];
    snprintf(command, sizeof(command), "DELETE %s %s\n", client->username, filename);
    
    if (send_command(client, command) < 0) {
        return -1;
    }

    char response[1024];
    if (receive_response(client, response, sizeof(response)) < 0) {
        return -1;
    }

    printf("Delete response: %s", response);
    
    return (strncmp(response, "OK", 2) == 0) ? 0 : -1;
}

int list_files(ClientState* client) {
    char command[256];
    snprintf(command, sizeof(command), "LIST %s\n", client->username);
    
    if (send_command(client, command) < 0) {
        return -1;
    }

    char response[1024];
    if (receive_response(client, response, sizeof(response)) < 0) {
        return -1;
    }

    printf("List response: %s", response);
    
    return (strncmp(response, "OK", 2) == 0) ? 0 : -1;
}

void interactive_mode(ClientState* client) {
    char input[512];
    char command[64];
    char arg1[256];
    char arg2[256];
    
    printf("\n=== File Storage Client ===\n");
    printf("Available commands:\n");
    printf("  signup <username> <password>  - Create new account\n");
    printf("  login <username> <password>   - Login to account\n");
    printf("  upload <filename>            - Upload file\n");
    printf("  download <filename>          - Download file\n");
    printf("  delete <filename>            - Delete file\n");
    printf("  list                         - List files\n");
    printf("  quit                         - Exit\n\n");
    
    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = '\0';
        
        if (strlen(input) == 0) {
            continue;
        }
        
        int parsed = sscanf(input, "%63s %255s %255s", command, arg1, arg2);
        
        if (strcmp(command, "quit") == 0) {
            send_command(client, "QUIT\n");
            break;
        } else if (strcmp(command, "signup") == 0) {
            if (parsed < 3) {
                printf("Usage: signup <username> <password>\n");
                continue;
            }
            signup(client, arg1, arg2);
        } else if (strcmp(command, "login") == 0) {
            if (parsed < 3) {
                printf("Usage: login <username> <password>\n");
                continue;
            }
            login(client, arg1, arg2);
        } else if (strcmp(command, "upload") == 0) {
            if (parsed < 2) {
                printf("Usage: upload <filename>\n");
                continue;
            }
            upload_file(client, arg1);
        } else if (strcmp(command, "download") == 0) {
            if (parsed < 2) {
                printf("Usage: download <filename>\n");
                continue;
            }
            download_file(client, arg1);
        } else if (strcmp(command, "delete") == 0) {
            if (parsed < 2) {
                printf("Usage: delete <filename>\n");
                continue;
            }
            delete_file(client, arg1);
        } else if (strcmp(command, "list") == 0) {
            list_files(client);
        } else {
            printf("Unknown command: %s\n", command);
        }
    }
}

void cleanup_client(ClientState* client) {
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
    }
}

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    int port = 9090;
    
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    
    printf("Connecting to server at %s:%d\n", host, port);
    
    int sockfd = connect_to_server(host, port);
    if (sockfd < 0) {
        printf("Failed to connect to server\n");
        return 1;
    }
    
    ClientState client = {
        .socket_fd = sockfd,
        .username = {0},
        .logged_in = 0
    };
    
    interactive_mode(&client);
    cleanup_client(&client);
    
    printf("Client disconnected\n");
    return 0;
}
