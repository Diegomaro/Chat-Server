## About The Project
A TCP chat server written in C++ to explore networking, protocol design, systems programming and client-server architecture. This repository serves as a long-term systems programming project that incrementally explores concepts in networking, security, system design, testing and performance.

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

## Resources
 * [Beej’s Guide to Network Programming](https://beej.us/guide/bgnet/)
 * [C++ reference](https://cppreference.com/)
 * [Man pages](https://man7.org/linux/man-pages/)