Build
-----

Linux/macOS:
- Requires POSIX threads.
- Run: `make`

Windows:
- Use MSYS2 or WSL to build with POSIX `pthread` and BSD sockets.
- Alternatively, port to WinSock and Win32 threads.

Run
---
`./server 9090`

Client
------
`./client [host] [port]`

See `CLIENT_README.md` for detailed client usage.

Protocol (temporary, line-based):
- SIGNUP <user> <pass>
- LOGIN <user> <pass>
- UPLOAD <user> <relpath> <size> <tmp_src_path>
- DOWNLOAD <user> <relpath>
- DELETE <user> <relpath>
- LIST <user>

Design
------
- Main thread accepts sockets, enqueues `ClientInfo*` to a client queue.
- Client thread pool parses commands and enqueues `Task*` to a global task queue.
- Worker thread pool executes filesystem ops and enqueues `Response*` into the per-client response queue entry.
- Client threads block on their response queue and write back to the socket.

Testing
-------
**Memory Leaks & Race Conditions:**

```bash
# Test for memory leaks
./testing/run_valgrind_simple.sh

# Test for race conditions (automated)
./testing/run_tsan_simple.sh
```

See `testing/README.md` for complete testing documentation.

**Test Reports:** All reports are saved in `reports/` directory.


