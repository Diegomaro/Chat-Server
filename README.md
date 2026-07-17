# Chat-Server

This repository is dedicated to learning how network programming is done on C++ and how to establish connections between multiple clients through a server.

### **How to use this project:**
This project was made on Debian and will only work with Linux systems.
```bash
git clone (https://github.com/Diegomaro/Chat-Server.git)
cd Chat-Server
. compile.sh
```
Run each command on different terminals.
```bash
./build/server
./build/client
```

### **The goals of this project are the following:**
1. [x] Send message from client to server and respond with acknowledgement.
2. [x] Keep session open between client and server until a specific input occurs.
3. [x] Send messages from multiple clients to server.
4. [x] Buffering of client messages. Store messages that are not ready to be sent on server.
5. [x] Redirect traffic from one client to another.
6. [x] Send confirmation of received messages by clients to original client.
7. [ ] Give each client a unique username and password defined when first accesing the program. Store credentials locally and on server bank (username + hash of password).
8. [ ] Make conversations work based on petitions from client to client.
9. [ ] Allow clients to login instead of registering.
10. [ ] Store data in local files. Server stores messages until they have been received and acknowledged by receiving party.

### **Resources:**
 - Beej’s Guide to Network Programming: https://beej.us/guide/bgnet/
 - C++ reference: https://cppreference.com/
 - Man pages: https://man7.org/linux/man-pages/

### **Goals if I continue the project:**
1. GUI of chat conversations.
2. Some sort of session implementation to avoid logging in each time the client program runs.
3. Encryption of messages.
4. Group creation.
5. Send files between clients.