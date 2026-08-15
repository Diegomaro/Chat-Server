# Centralized Messaging Protocol (CMPv2)

## Abstract

## Table of Contents

## 1. Purpose and Scope
This document specifies the utility and composition of a CMPv2 message. The purpose of this document is to have a clear explanation on what each field does and how the different messages are processed to allow for any programmer to implement a client from only this document. It also serves the purpose of keeping track of design decisions regarding the protocol and a general overview of what it must achieve, to ensure the protocol fulfills its goals successfully. The scope of this document is to specify how to handle CMPv2 messages and not how to implement a server or a client for any protocol, that implementation relies on the programmer and can take many shapes and forms. The protocol simply describes how the messages should be constructed and what each type of server response means.
## 2. Introduction
Protocol.md contains a large amount of design goals and specifications of the CMPv2 protocol, specifically what each message does and how the connection and authentication are managed. This is not the only document of this project, more practical specifications are described on architecture.md and a general road map of features to be implemented is found on roadmap.md.
### 2.1. Terminology
Work on this later.
## 3. Functional Specification
### 3.1. Message Format
CMP messages are formatted as follows:
```text
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Header    |   Payload    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
The header is fixed in size and is followed by a variable-length payload.
#### 3.1.1. Header Format
CMP messages are sent over TCP. Each CMP header, followed by the message is formatted as follow:
```text
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Version    |     Type      |          Client Key          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Client Key          |          Message ID          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Message ID                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Message ID          |           Timestamp          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Timestamp           |        Payload Length        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
 Note that one tick mark represents one bit position.
			Figure 1: CMP Header Format
where:
**Version:** 8 bits
	The version of the protocol.
	 Version = 2
**Type:** 8 bits
	The type of the message. The types are explained in more detail on [[#3.2 Message Types]].
**Client Key:** 32 bits
	The identification key of  a client. Its interpretation as a sender or receiver depends on the message type. Valid values range from 0 to 0xFFFFFFFE. The last one is reserved. The keys are distributed by the server after a successful registration. They belong to the registered user permanently.
**Message ID:** 64 bits
	The ID of the message, consists of some host bits, and some unique number bits.
```text
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                Message ID               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Client Key     |       Number       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
Figure 3: Message ID subdivision.
	**The Client Key:** 32 bits
	**Unique Number:** 32 bits
	The message consists of the client's Client Key and a client generated unique number. The ID must remain unique until a message has stopped being processed.
**Timestamp:** 32 bits
	The date and time at which the message was captured + processed by the server. The time is set in UTC. Clients should not modify the timestamp from its default values. Which are all set to 0. Server overwrites the value regardless whether they are set to 0, or any other value.
```
text
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
|                         Timestamp                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
|   Minute   |   Hour   |   Day   |   Month   |   Year   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
```
Figure 4: Timestamp subdivision
	**Minute:** 6 bits
	**Hour:** 5 bits
	**Day:** 5 bits
	**Month:** 4 bits
	**Year:** 12 bits
**Payload Length:** 16 bits
	The total length of the message payload. The details of how the length of a payload are determined are reviewed on [[#3.1.2. Payload Format]].
#### 3.1.2. Payload Format
The payload length is measured in bytes and is allowed to be empty for certain types of messages. The payload data starts right after payload length. The maximum CMP message size is 65536, including the  20 byte header. The NULL terminator is not included. The contents of the different types of payload are described on [[#3.2 Message Types]].
#### 3.1.3. Byte Order
All multi-byte integer fields are encoded in network byte order (big-endian).
#### 3.1.4. Reserved values
| Field      | Reserved Value | Meaning                |
| ---------- | -------------- | ---------------------- |
| Client Key | 0xFFFFFFFF     | Invalid receiver       |
| Version    | 0              | Invalid version number |
| Type       | 0              | Invalid type           |
	Table 1.
### 3.2. Message Types
#### 3.2.1. USER
**Purpose:** Send message to another client.
**Payload:** From 1 to the max payload size, defined in [[#3.1.2. Payload Format]].
**Receiver:** The client specifies the recipient using their Client Key. When forwarding a message,  the server replaces the Client Key field with the sender's Client Key.
**Expected Response:** The server sends the payload to the specified receiver.
#### 3.2.2. REGISTER
**Purpose:** Send user registration request.
**Payload:** Includes a 16 byte username with a variable length password from 8 to 60 bytes. The username is fixed sized, unoccupied characters are set to NULL. The password starts right after the 16 byte username.
**Receiver:** The server, the receiver key is set to 0xFFFFFFFF.
**Expected Response:** The server receives the credentials and processes them.
#### 3.2.3. LOGIN
Not defined yet.
#### 3.2.4. SEND_REQUEST
**Purpose:** Send a request to communicate to another user by their username.
**Payload:** Includes a 16 byte username. The server replaces it with the sender's username. The server must store Client Key - username mappings to do this process.
**Receiver:** An unknown receiver identified solely by their username, their key remains unknown.
**Expected Response:** The request reaches the receiver with the sender's username + key.
#### 3.2.5. ACCEPT_REQUEST
**Purpose:** Sends an accept message to a user that requested communication.
**Payload:** Includes a 16 byte username of the sender.
**Receiver:** A receiver identified by their key.
**Expected Response:** The accept message reaches the receiver and both sender and receiver register the other user username + key.
#### 3.2.6. REJECT_REQUEST
**Purpose:** Sends a reject message to a user that requested communication.
**Payload:** Includes a 16 byte username of the sender.
**Receiver:** A receiver identified by their key.
**Expected Response:** The rejection message reaches the receiver.
#### 3.2.7. INFO
**Purpose:** The server sends an information message to a user.
**Payload:** 1 byte for the different types of informational messages.
**Receiver:** A connected client.
**Expected Response:** No response is expected, they are solely informational.
##### 3.2.7.1. Info Types
The sender for these messages is the server, the receiver is the client who message is being validated and processed.
###### 3.2.7.1.1. VALID_REGISTER
**Purpose:** The server tells the client that their credentials were valid and they were successfully registered.
**Expected Response:** Varies between types.
###### 3.2.7.1.2. INVALID_CREDENTIAL
**Purpose:** The server tells the client that they credentials were invalid, they did not follow the specified rules. These rules are:
* Usernames must be at least one character long and less than or equal to 16 characters.
* Usernames must only contain letters of the English alphabet, numbers or underscore.
* Passwords must be at least 8 characters long and less or equal than 60 characters.
* more rules to be defined later.
###### 3.2.7.1.3. NOT_UNIQUE
**Purpose:** The server tells the client that they credentials were valid, but the selected username has been occupied by another client.
###### 3.2.7.1.4. ALREADY_LOGGED_IN
**Purpose:** The server tells the client that they have have an active session and cannot register again.
###### 3.2.7.1.5. INVALID_MESSAGE
**Purpose:** The server tells the client that the message sent does not follow the protocol rules.
###### 3.2.7.1.6. INVALID_CLIENT
**Purpose:** The server tells the client that the specified receiver is invalid. Either because it does not exist, or they are not allowed to communicate to that client.
###### 3.2.7.1.7. ALREADY_SENT_REQUEST
**Purpose:** The server tells the client that the specified receiver in a SEND_REQUEST has already a pending request from them.
###### 3.2.7.1.8. ALREADY_KNOWN_CLIENT
**Purpose:** The server tells the receiver for which they requested communication that they already established a trusted connection.
###### 3.2.7.1.9. REQUEST_ALREADY_RECEIVED
**Purpose:** The server tells the client that the specified receiver in a SEND_REQUEST has already sent them a request.
###### 3.2.7.1.10. UNAUTHENTICATED_USER
**Purpose:** The server tells the client that they must authenticate before sending a message of that type.
###### 3.2.7.1.11. SEND_ERROR
**Purpose:** The server tells the client that the server could not send their message at that specific moment.
###### 3.2.7.1.12. COULD_NOT_REGISTER
**Purpose:** The server tells the client that they could not register them, the maximum client per server has been reached.
#### 3.2.8. UPDATE
To be defined later.
#### 3.2.9. ACK
Not complete, to be defined later.
### 3.3. Protocol State
#### 3.3.1. Connection States
**Connected**
The client has established a TCP connection with the server but has not yet been authenticated.

**Authenticated**
The client has successfully authenticated and is allowed to send requests and messages.

**Disconnected**
The client is not connected to the server.
#### 3.3.2. State Machine
```text
+--------------+
| DISCONNECTED |
+--------------+
       |
       | TCP connect
       v
+--------------+
|   CONNECTED  |
+--------------+
       |
       | REGISTER / LOGIN
       |
       | success
       v
+--------------+
| AUTHENTICATED|
+--------------+
       |
       | TCP close
       v
+--------------+
| DISCONNECTED |
+--------------+
```
Figure x. Client connection state machine
### 3.4. Connection Management
This section defines the procedures to establish, authenticate and terminate a CMP connection. CMP operates over TCP, and the CMP connection state works independently of the underlying TCP connection state.
#### 3.4.1. Establishing a Connection
A CMP connection is established over TCP. The client initiates the TCP connection to the server.
The connection establishment procedure is the following:
1. The client establishes a TCP connection with the server.
2. The server accepts the TCP connection.
3. The client enters the 'CONNECTED' CMP state.
4. The client may send authentication messages.
After the connection enters the 'CONNECTED' state, the client is considered unauthenticated until a valid registration or login procedure has been completed. If the TCP connection is not established, the client may attempt to establish a new TCP connection with the server.
#### 3.4.2. Authentication
##### 3.4.2.1. Registration
A client in the 'CONNECTED' state may request registration by sending a 'REGISTER' message.
The registration procedure is the following:
1. The client sends a 'REGISTER' message to the server containing their credentials (username and password).
2. The server validates the message format.
3. The server validates the individual credentials by length and character validity.
4. The server checks whether the username is available.
5. The server registers the USER and assigns a Client Key for the registered user.
6. The server sends a 'VALID_REGISTER' information message with the Client Key field being their newly assigned Client Key.
7. The client enters the 'AUTHENTICATED' state.
If the registration fails at any point, the server sends an information message of the following types: INVALID_CREDENTIAL, ALREADY_LOGGED_IN, COULD_NOT_REGISTER, and NOT_UNIQUE.
##### 3.4.2.2. Login
To be defined later.
#### 3.4.3. Connection Termination
A CMP connection may be terminated by either the client or the server. Connection termination is performed by closing the TCP connection.
When the TCP connection is closed:
1. The connection enters the 'DISCONNECTED' state on the client.
2. Resources associated with the client are released.
3. The user's authenticated session is terminated.
#### 3.4.4. Message/state matrix
| Message        | Disconnected | Connected   | Authenticated |
| -------------- | ------------ | ----------- | ------------- |
| REGISTER       | No           | Yes         | No            |
| LOGIN          | No           | Yes         | No            |
| USER           | No           | No          | Yes           |
| SEND_REQUEST   | No           | No          | Yes           |
| ACCEPT_REQUEST | No           | No          | Yes           |
| REJECT_REQUEST | No           | No          | Yes           |
| INFO           | Server only  | Server only | Server only   |
| UPDATE         | No           | -           | -             |
| ACK            | No           | -           | -             |
### 3.5. Data Communication
#### 3.5.1. Message Reception
CMP Messages are received by the server and stored. As CMP is built on top of TCP, one message can be split or combined with another message, so the server must be able to store messages regardless of whether the received message is valid or not.
#### 3.5.2. Message Validation
After each capture by the server, the message should be validated. Validation happens in two stages.
##### 3.5.2.1. Header Validation
The header is read byte per byte and processed accordingly. The version must coincide with the current version used by the server. The type of the message must be among the defined types. The payload length must be less or equal than the maximum payload length. The Client Key, if not set to 0xFFFFFFFF must be a registered key of a client. Once the header has been validated, the server must await until the number of bytes received is equal or greater than the payload length + HEADER_SIZE.
##### 3.5.2.2. Payload Length validation
Once these two steps have been completed successfully. The server must validate whether the payload length matches the allowed payload length of the selected message type.
#### 3.5.3. Message Processing
Once the message has been validated, the server must do different tasks according to the specified message type. These are described on [[#3.2. Message Types]]. The following are the methods to process each type of message:
##### 3.5.3.1. USER Processing
1. Verify that the client is authenticated.
2. Verify that the Client Key identifies a valid client.
3. Verify that the receiver is known by the sender.
4. Forward the message to the receiver.
5. Send a "processed ACK" to the sender.
##### 3.5.3.2. REGISTER Processing
1. Verify that the client is not authenticated.
2. Verify the credentials total length.
3. Verify that the username and password only contain valid characters.
4. Verify the password length.
5. Check whether the selected username does not belong to another client.
6. Generate the next key to give to the client.
7. Mark client as logged in, and store necessary information.
8. Send an INFO message with the generated Client Key.
##### 3.5.3.3. SEND_REQUEST Processing
1. Verify the credential length.
2. Verify that the username only contains valid characters.
3. Verify that the username is not the sender's username.
4. Verify that the requested username exists.
5. Verify that the sender does not already know the receiver.
6. Verify that the sender does not already have a pending request for the receiver.
7. Verify that the sender does not already have a pending request from the receiver.
8. Add the request to the pending requests list of the sender.
9. Forward the message to the receiver.
##### 3.5.3.4. ACCEPT_REQUEST Processing
1. Verify the credential length.
2. Verify that the username only contains valid characters.
3. Verify that the requested username exists.
4. Verify that the sender does not already know the receiver.
5. Verify that the sender has a pending request from the receiver.
6. Forward the message to the receiver.
7. Add the Client Key of each client to the other client list of known clients.
8. Delete the pending request.
##### 3.5.3.5. REJECT_REQUEST Processing
1. Verify the credential length.
2. Verify that the username only contains valid characters.
3. Verify that the requested username exists.
4. Verify that the sender does not already know the receiver.
5. Verify that the sender has a pending request from the receiver.
6. Forward the message to the receiver.
7. Delete the pending request.
##### 3.5.3.6. UPDATE Processing
To be defined later.
##### 3.5.3.7. ACK Processing
Work in progress.
#### 3.5.4. Acknowledgements
Work in progress.
### 3.6. Error Handling
Errors that occurred during message reception, message validation and message processing are handled by informational messages sent by the server. The different types of error that can occur are defined on [[#3.2.7.1. Info Types]]. Server side errors depend on implementation and are not defined in this document.
## 4. Security Considerations

Add later.
## 5. Protocol Constants
The following constants define fixed values and limits used by CMPv2.

| Constant            | Value      | Description                      |
| ------------------- | ---------- | -------------------------------- |
| CMP_VERSION         | 2          | Current CMP protocol version     |
| MAX_MESSAGE_SIZE    | 65536      | Maximum CMP message size         |
| MAX_PAYLOAD_SIZE    | 65516      | Maximum CMP payload size         |
| CLIENT_KEY_INVALID  | 0xFFFFFFFF | Reserved Client Key              |
| USERNAME_LENGTH     | 16         | Fixed size of the client's name  |
| MIN_PASSWORD_LENGTH | 8          | Mimimum size of a valid password |
| MAX_PASSWORD_LENGTH | 60         | Maximum size of a valid password |
| HEADER_SIZE         | 20         | Fixed CMP header size            |
| CLIENT_KEY_LENGTH   | 4          | Fixed size of the Client Key     |
### 5.1. Message Type Values
| Value | Type           |
| ----- | -------------- |
| 0     | INVALID_TYPE   |
| 1     | USER           |
| 2     | REGISTER       |
| 3     | LOGIN          |
| 4     | SEND_REQUEST   |
| 5     | ACCEPT_REQUEST |
| 6     | REJECT_REQUEST |
| 7     | INFO           |
| 8     | UPDATE         |
| 9     | ACK            |
### 5.2. Message Info Values
| Value | Type                     |
| ----- | ------------------------ |
| 1     | VALID_REGISTER           |
| 2     | INVALID_CREDENTIAL       |
| 3     | NOT_UNIQUE               |
| 4     | ALREADY_LOGGED_IN        |
| 5     | INVALID_MESSAGE          |
| 6     | INVALID_CLIENT           |
| 7     | ALREADY_SENT_REQUEST     |
| 8     | ALREADY_KNOWN_CLIENT     |
| 9     | REQUEST_ALREADY_RECEIVED |
| 10    | UNAUTHENTICATED_USER     |
| 11    | SEND_ERROR               |
| 12    | COULD_NOT_REGISTER       |
## 6. Implementation Notes
### 6.1. Server Shutdown
The current implementation terminates the server process when a client
connection is closed. This behavior is temporary and is not part of
the CMPv2 specification.
### 6.2. TCP Stream Handling

### 6.3. Buffering

### 6.4. Resource Limits

## 7. Examples
### 7.1. Registration
### 7.2. Login
### 7.3. Sending a Message
### 7.4. Acknowledgements

## 8. References
1. https://datatracker.ietf.org/doc/html/rfc9293