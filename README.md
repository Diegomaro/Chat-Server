# Chat-Server

This repository is dedicated to learning how network programming is done on C++ and how to establish connections between multiple clients through a server. The expanded purpose of this project is to serve as a default project to apply new concepts learned from various software development areas.

### **How to use this project:**
This project was made on Debian and will only work with Linux systems.
```bash
git clone https://github.com/Diegomaro/Chat-Server.git
cd Chat-Server
./compile.sh
```
Run each command on different terminals.
```bash
./build/server
./build/client
```

### **The goals of this project are the following:**
#### 1.0 Version
1. [x] Send message from client to server and respond with acknowledgement.
2. [x] Keep session open between client and server until a specific input occurs.
3. [x] Send messages from multiple clients to server.
4. [x] Buffering of client messages. Store messages that are not ready to be sent on server.
5. [x] Redirect traffic from one client to another.
6. [x] Send confirmation of received messages by clients to original client.
7. [x] Give each client a unique username and password defined when first accessing the program.
8. [ ] Make conversations work based on petitions from client to client.
9. [ ] Input validation.
10. [ ] Allow clients to login instead of registering.

#### 2.0 Version
1. [ ] Store credentials locally and on server bank (username + hash of password).
2. [ ] Store data in local files. Server stores messages until they have been received and acknowledged by receiving party.
3. [ ] Print entire chat conversations to a file.
4. [ ] Load conversations on request.
5. [ ] Server safe shutdown and restore system.
6. [ ] Priority based server resending of messages based on length/type of messages.
7. [ ] Restrict amount of data being able to be received by server from a specific client (cooldowns).
8. [ ] Unit testing.
9. [ ] Event logging.

#### 3.0 Version
1. [ ] Session token with expiration.
2. [ ] Transit encryption of messages.
3. [ ] Rest encryption of messages and key storage.
4. [ ] Group creation.
5. [ ] Send files between clients.
6. [ ] Split terminal window.

#### 4.0 Version
1. [ ] Fuzz testing.
2. [ ] Integration testing.
3. [ ] Resilience testing.

#### 5.0 Version
1. [ ] System status reporting.
2. [ ] Admin accounts.

#### 6.0 Version
1. [ ] Stress testing.
2. [ ] Windows compatibility.

#### 7.0 Version
1. [ ] GUI of application.

### **Resources:**
 - Beej’s Guide to Network Programming: https://beej.us/guide/bgnet/
 - C++ reference: https://cppreference.com/
 - Man pages: https://man7.org/linux/man-pages/