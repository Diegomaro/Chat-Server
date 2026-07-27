## About The Project
A TCP chat server written in C++ to explore networking, protocol design, systems programming and client-server architecture. This repository serves as a long-term systems programming project that incrementally explores concepts in networking, security, system design, testing and performance.

## Project Goals
THe goal of thhis project is to learn the fundamentals of network programming, security and understand the design principles behind reliable client-server applications.

The project focuses on:
* TCP/IP networking
* Software architecture
* Reliable messaging
* Memory management
* Security fundamentals
* Testing

## Design Principles
Some guiding principles include:
- Build core features from scratch.
- Prioritize reliability over optimization.
- Consider scalability throughout development.

## Getting Started
### Prerequisites
- Linux
- C++ compiler with C++17 support

### Installation
1. Clone the repository and enter the project directory.
```sh
git clone https://github.com/Diegomaro/Chat-Server.git
cd Chat-Server
```
2. Build the project
```sh
. compile.sh
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

## Current Features
* TCP client/server communication.
* Multiple simultaneous client connections.
* Message forwarding between clients.
* Buffering of incoming and outgoing messages.
* Unique user registration.

## Roadmap

### Milestone 1
1. [x] Send message from client to server and respond with acknowledgement.
2. [x] Keep session open between client and server until a specific input occurs.
3. [x] Send messages from multiple clients to server.
4. [x] Buffering of client messages. Store messages that are not ready to be sent on server.
5. [x] Redirect traffic from one client to another.
6. [x] Send confirmation of received messages by clients to original client.
7. [x] Give each client a unique username and password defined when first accessing the program.
8. [ ] Make conversations work based on requests from client to client.
9. [ ] User input validation.
10. [ ] Allow clients to login instead of registering.

### Milestone 2
1. [ ] Store credentials locally and on server storage (username + hash of password).
2. [ ] Store data in local files. Server stores messages until they have been received and acknowledged by receiving party.
3. [ ] Export chat conversations to a file.
4. [ ] Load conversations on request.
5. [ ] Graceful shutdown and server system restore.
6. [ ] Prioritized message retransmission.
7. [ ] Rate limiting per client.
8. [ ] Unit testing.
9. [ ] Event logging.

### Milestone 3
1. [ ] Encryption in transit.
2. [ ] Encryption at rest and key storage.
3. [ ] Session token with expiration.
4. [ ] Group creation.
5. [ ] Send files between clients.
6. [ ] Split terminal window into message printing and user input.

### Milestone 4
1. [ ] Fuzz testing.
2. [ ] Integration testing.
3. [ ] Resilience testing.

### Milestone 5
1. [ ] System status reporting.
2. [ ] Admin accounts.

### Milestone 6
1. [ ] Stress testing.
2. [ ] Windows compatibility.

### Milestone 7
1. [ ] GUI of application.

## Resources
 * [Beej’s Guide to Network Programming](https://beej.us/guide/bgnet/)
 * [C++ reference](https://cppreference.com/)
 * [Man pages](https://man7.org/linux/man-pages/)