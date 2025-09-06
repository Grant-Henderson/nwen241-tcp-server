# TCP File Server in C

This project implements a simple TCP server written in C.  
It was originally built as coursework for NWEN241 but is packaged here as a demonstration of **socket programming** on Linux.

## Features
- Accepts a port number as a command-line argument (must be ≥ 1024).
- Sends `HELLO` when a client connects.
- Handles commands (case-insensitive):
  - `BYE` → closes the connection.
  - `GET <filename>` → returns the contents of the file, or an error if not found.
  - `PUT <filename>` → writes client data to a file until **two consecutive blank lines** are received.
  - Unknown command → `SERVER 502 Command Error`.

## Versions
- **server.c** → single-client version.
- **server2.c** → enhanced version that uses `fork()` to handle multiple clients.

## Build
Compile with `gcc`:
```bash
gcc -Wall -Wextra -o server server.c
gcc -Wall -Wextra -o server2 server2.c
