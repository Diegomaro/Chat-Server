## About The Project
A TCP chat server written in C++ to explore networking, protocol design, systems programming and client-server architecture. This repository serves as a long-term systems programming project that incrementally explores concepts in networking, security, system design, testing and performance.

## Getting Started
### Prerequisites
- Linux
- gcc compiler with C++17 support

### Installation
1. Clone the repository and enter the project directory:
```sh
git clone https://github.com/Diegomaro/Chat-Server.git
cd Chat-Server
```
2. Build the project:
```sh
./compile.sh
```

### Usage
Start the server:
```sh
./build/server
```
In other terminals, start one or more clients:
```sh
./build/client
```

## Roadmap

### Milestone 1
[x] Send message from client to server and respond with acknowledgement.
[x] Keep session open between client and server until a specific input occurs.
[x] Send messages from multiple clients to server.
[x] Buffering of client messages. Store messages that are not ready to be sent on server.
[x] Redirect traffic from one client to another.
[x] Send confirmation of received messages by clients to original client.
[x] Give each client a unique username and password defined when first accessing the program.
[x] Make conversations work based on requests from client to client.
[x] User input validation.
[] Handle invalid requests and malformed messages.
[] Handle edge cases and system failures.

### Milestone 2
[ ] Allow clients to login instead of registering.
[ ] Store credentials locally and on server storage (username + hash of password).
[ ] Store data in local files. Server stores messages until they have been received and acknowledged by receiving party.
[ ] Export chat conversations to a file and load on request.
[ ] Configuration file.
[ ] Event logging.
[ ] Unit testing.

### Milestone 3
[ ] ID system of messages.
[ ] Priority message queue.

### Milestone 4
[ ] Graceful shutdown and server system restore.
[ ] Detect client disconnections.
[ ] Client reconnect.
[ ] Retransmission of undelivered messages.

### Milestone 5
[ ] Rate limiting per client.
[ ] Session token with expiration.

### Milestone 6
[ ] Encryption in transit.
[ ] Encryption at rest and key storage.

### Milestone 7
[ ] Fuzz testing.
[ ] Integration testing.
[ ] Resilience testing.

### Milestone 8
[ ] Windows compatibility.
[ ] Stress testing.

### Milestone 9
[ ] System status reporting.
[ ] Admin accounts.

### Milestone 10
[ ] File sending.
[ ] Group creation.

### Milestone 11
[ ] Split terminal window into message printing and user input.
[ ] GUI of application.

## Resources
 * [Beej’s Guide to Network Programming](https://beej.us/guide/bgnet/)
 * [C++ reference](https://cppreference.com/)
 * [Man pages](https://man7.org/linux/man-pages/)