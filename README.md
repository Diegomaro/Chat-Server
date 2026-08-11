![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Release](https://img.shields.io/github/v/release/Diegomaro/Chat-Server?include_prereleases)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

## About The Project
A TCP chat server written in C++ to explore networking, protocol design, systems programming and scalable client-server architecture. This repository serves as a long-term systems programming project that incrementally explores concepts in networking, security, system design, testing and performance.

## Features
Current features:
- Message protocol.
- Input validation.
- Simple user menu to authenticate and send messages.
- Simple authentication.
- Simultaneous client connectivity.
- One-to-one communication with connection request/accept/reject.
- Dedicated memory space for message storage.
- Handling of malformed client messages.
- Delivered message acknowledgements.

## Program demonstration

https://github.com/user-attachments/assets/1785b745-a3d2-4328-a1b2-71022ad7f0b5

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

## Program structure
docs/
- [Architecture](docs/architecture.md)
- [Protocol](docs/protocol.md)
- [Roadmap](docs/roadmap.md)
- ...

headers/
- constants/

src/
- client/
- server/

## Resources
 * [Beej’s Guide to Network Programming](https://beej.us/guide/bgnet/)
 * [C++ reference](https://cppreference.com/)
 * [Man pages](https://man7.org/linux/man-pages/)