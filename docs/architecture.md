# Architecture

## Overview
The project implements a basic TCP client-server messaging service. Clients authenticate with the server before establishing communication with other users. The server is responsible for authentication, authorization, connection management and packet routing.

## Components

### Client

#### Responsibilities
- Establish a TCP connection with the server.
- Serialize outgoing packets.
- Deserialize incoming packets.
- Display received messages.
- Handle user input.
- Store information about known users.
- Send acknowledgements of received messages.

#### Ownership
- Incoming buffer.
- Outgoing buffer.
- Information about known clients.

### Server

#### Responsibilities
- Accept incoming TCP connections.
- Authenticate clients.
- Authorize client-to-client communication.
- Route packets between clients.
- Manage message buffering.

#### Ownership
 - Shared incoming buffer.
 - Shared outgoing buffer.
 - Client information.
 - Client relationships.
 - Client requests.

## Data Structures

### Client
- Hash Table - username_to_key
	- Maps usernames to client keys for fast username lookup. Reverse lookups are performed by iterating over the table.
 - Linked List - incoming_requests
	 - Adding, removing and traversing communication requests from other users.
### Server
- Hash Table - clients
	- Stores connected client objects indexed by their socket file descriptor, easily accessed with the use of epoll.
- Hash Table - client_key_to_socket
	- Fast conversion from key to socket.
- Hash Table username_to_client_key
	- Fast conversion from username to client key to direct requests of communication between users.
- Hash Table - client_key_to_known_keys
	- Fast access to all the known keys of a specific user.
- Hash Table - client_key_to_request_keys
	- Fast access to all the requests of a specific user.
- Linked List - available_buffers
	- Distribution of buffers to clients with O(1) complexity.
## Communication Flow
### Establishment
1. Client establishes TCP connection.
2. Client sends authentication credentials.
3. Server authenticates the client.
4. Client sends a communication request to another client.
5. Server validates the request and forwards it.
6. Receiving client accepts the request and sends a response to the server.
7. Server validates the request and forwards it.
8. Original client receives the accepted request.
9. Both clients can communicate with each other.
### Messaging
1. Client 1 sends message to Client 2.
2. Server validates that Client 1 knows Client 2.
3. Server forwards the message.
4. Server sends processed acknowledgement to Client 1.
5. Client 1 receives processed acknowledgement.
6. Client 2 receives sender's message.
7. Client 2 sends delivered acknowledgement to server.
8. Server validates and forwards acknowledgement to Client 1.
## Resource ownership
The server owns the shared resources. Whenever a client connects, the server assigns them one 512-byte buffer segment. Each client can request more buffer segments if needed, up to 128 segments. Allocation and deallocation are performed exclusively by the server. Clients release segments by returning them to the shared pool. Long messages are split across multiple segments.

## Design Decisions

### Shared Buffer Pool
Instead of allocating memory for each client, the server maintains a shared pool with a max user capacity. This reduces memory allocations and deallocations during runtime.
### Hash Table Lookup
The server was constructed with the idea of eventually being able to manage one million users. Because of this, most lookups are done through a hash table to have an average lookup complexity of O(1).
### Reduced Client Responsibility
The authentication and authorization are performed by the server. This prevents clients from bypassing authentication, which could lead to username spoofing. It also prevents bypassing authorization, which could lead to spam, malicious messaging, server saturation.
### Dual Acknowledgement
Every message generates two acknowledgements that allow the sender to know when their messages have been processed and if they were delivered successfully.
### Message Status Reporting
Invalid messages are reported back to the client, this ensures client can restore functionality in case they sent a malformed message.
The implemented status reports are:
- VALID_REGISTER
	- The client was registered successfully.
- INVALID_CREDENTIAL
	- One of the submitted credentials was invalid.
- NOT_UNIQUE
	- The username already exists.
- ALREADY_LOGGED_IN
	- The client is already logged in.
- INVALID_MESSAGE
	- Packet format is invalid.
- INVALID_CLIENT
	- Unknown or non-existent client.
- ALREADY_SENT_REQUEST
	- A request to that client had already been sent.
- ALREADY_KNOWN_CLIENT
	- The requested client is already known.
- UNAUTHENTICATED_USER
	- The client is not authenticated.
- SEND_ERROR
	- The message could not be delivered.
- COULD_NOT_REGISTER
	- Registration failed, client capacity reached.
