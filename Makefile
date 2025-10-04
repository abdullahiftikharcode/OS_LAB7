CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -std=c11 -O2 -pthread
LDFLAGS=-pthread

SRC=src/main.c src/queue.c src/user.c src/fs.c src/client.c src/worker.c
CLIENT_SRC=src/client_program.c
INC=-Iinclude

BIN=server
CLIENT_BIN=client

all: $(BIN) $(CLIENT_BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC) $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC)

clean:
	rm -f $(BIN) $(CLIENT_BIN)

.PHONY: all clean

