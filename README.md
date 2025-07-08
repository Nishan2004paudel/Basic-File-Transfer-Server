# Basic File Transfer Application

A simple TCP file transfer server and client implementation in C using Winsock2.

## Features

- Large file support (>4GB) using 64-bit operations
- Multi-threaded server for concurrent clients
- Real-time progress tracking
- Error handling and recovery
- Windows platform using Winsock2

## Project Structure

```
Basic File Transfer Application/
├── README.md
├── client/
│   ├── client.c
│   └── client.exe
└── server/
    ├── server.c
    ├── server.exe
    └── test.txt
```

## Prerequisites

- Windows 10/11
- GCC compiler (MinGW-w64) or Visual C++
- Winsock2 library

## Quick Start

### Compile

```bash
# Server
cd server
gcc -o server.exe server.c -lws2_32

# Client
cd client
gcc -o client.exe client.c -lws2_32
```

### Run

1. Start the server:

```bash
cd server
./server.exe
```

2. Run the client:

```bash
cd client
./client.exe
```

3. Enter the filename when prompted

## Usage

The server listens on port 12345 and serves files from its directory. The client connects to the server and requests files by name. Downloaded files are saved with a "received\_" prefix.

## Technical Details

- **Protocol**: TCP
- **Port**: 12345
- **Buffer Size**: 4096 bytes
- **File Size**: 64-bit support for large files
- **Threading**: Multi-threaded server design

**Made by**: Nishan and Rishav | **Course**: Network Programming Lab | **Semester**: 6th
