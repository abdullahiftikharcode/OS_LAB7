CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -std=c11 -O2 -pthread -DSQLITE_THREADSAFE=1
CFLAGS_DEBUG=-Wall -Wextra -Wpedantic -std=c11 -g -O0 -pthread -DSQLITE_THREADSAFE=1
CFLAGS_TSAN=-Wall -Wextra -Wpedantic -std=c11 -g -O1 -pthread -fsanitize=thread -DSQLITE_THREADSAFE=1
LDFLAGS=-pthread -lsqlite3
LDFLAGS_TSAN=-pthread -fsanitize=thread -lsqlite3

# Source files
SRC=src/main.c src/queue.c src/user_db.c src/fs.c src/client.c src/worker.c src/config.c src/task.c src/priority.c src/crypto.c
CLIENT_SRC=src/client_program.c
INC=-Iinclude

BIN=server
CLIENT_BIN=client
BIN_DEBUG=server_debug
BIN_TSAN=server_tsan
CLIENT_BIN_DEBUG=client_debug

all: $(BIN) $(CLIENT_BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC) $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC)

# Debug build for Valgrind (with debug symbols, no optimization)
debug: $(BIN_DEBUG) $(CLIENT_BIN_DEBUG)

$(BIN_DEBUG): $(SRC)
	$(CC) $(CFLAGS_DEBUG) $(INC) -o $@ $(SRC) $(LDFLAGS)

$(CLIENT_BIN_DEBUG): $(CLIENT_SRC)
	$(CC) $(CFLAGS_DEBUG) -o $@ $(CLIENT_SRC)

# ThreadSanitizer build
tsan: $(BIN_TSAN)

$(BIN_TSAN): $(SRC)
	$(CC) $(CFLAGS_TSAN) $(INC) -o $@ $(SRC) $(LDFLAGS_TSAN)

# Valgrind target
valgrind: debug
	@echo "Run: valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./server_debug 9090"

clean:
	rm -f $(BIN) $(CLIENT_BIN) $(BIN_DEBUG) $(CLIENT_BIN_DEBUG) $(BIN_TSAN)

.PHONY: all clean debug tsan valgrind

