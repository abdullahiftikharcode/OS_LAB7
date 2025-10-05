#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
	CMD_UPLOAD,
	CMD_DOWNLOAD,
	CMD_DELETE,
	CMD_LIST,
	CMD_SIGNUP,
	CMD_LOGIN,
	CMD_QUIT
} CommandType;

typedef struct {
	int client_id; // internal id for routing responses
	int socket_fd;
} ClientInfo;

typedef struct {
	CommandType type;
	ClientInfo client;
	char username[64];
	char password[64];
	char path[256];
	char tmpfile[256]; // for uploads
	size_t size;
} Task;

typedef enum {
	RESP_OK=0,
	RESP_ERR=1
} ResponseStatus;

typedef struct {
	int client_id;
	ResponseStatus status;
	char message[512];
	char filepath[256]; // for download
	size_t size;
} Response;

#endif

