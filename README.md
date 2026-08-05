## About The Project
A TCP chat server written in C++ to explore networking, protocol design, systems programming and client-server architecture. This repository serves as a long-term systems programming project that incrementally explores concepts in networking, security, system design, testing and performance.

## Features
The current implemented features:
- Message protocol.
- Input validation.
- Simple user menu to authenticate and send messages.
- Simple auhentication.
- Simultaneous client connectivity.
- One-to-one communications with connection request/accept/reject.
- Dedicated memory space for shared message buffering.
- Handling of malformed client messages.
- Delivered message acknowledgements.

## Program demonstration

https://github.com/user-attachments/assets/1785b745-a3d2-4328-a1b2-71022ad7f0b5

## Getting Started
### Prerequisites
- Linux
- gcc compiler with C++14 support

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

## Program structure
docs/
- Includes all documentation.
headers/
- Includes hpp and tpp files.
- constants/
  - Includes system configuration, return types, protocol header types and status types.
src/
- Includes cpp files in two directories, one for client and one for server.

## Resources
 * [Beej’s Guide to Network Programming](https://beej.us/guide/bgnet/)
 * [C++ reference](https://cppreference.com/)
 * [Man pages](https://man7.org/linux/man-pages/)