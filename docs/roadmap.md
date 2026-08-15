# Roadmap

## Milestone 1
1. [x] Send message from client to server and respond with acknowledgement.
2. [x] Keep session open between client and server until a specific input occurs.
3. [x] Send messages from multiple clients to server.
4. [x] Buffering of client messages. Store messages that are not ready to be sent on server.
5. [x] Redirect traffic from one client to another.
6. [x] Send confirmation of received messages by clients to original client.
7. [x] Give each client a unique username and password defined when first accessing the program.
8. [x] Make conversations work based on requests from client to client.
9. [x] User input validation.
10. [x] Handle invalid requests and malformed messages.
11. [x] Handle edge cases and system failures.

## Milestone 2
1. [ ] Intercommunication for little endian and big endian architectures.
2. [ ] Improved protocol header.
3. [ ] ID system of messages.
4. [ ] Reworked acknowledgement system to work with message IDs.
5. [ ] Server time synchronization with NTP.
6. [ ] Timestamps.
7. [ ] Protocol versioning.
8. [ ] Priority message queue.

## Milestone 3
1. [ ] Allow clients to login instead of registering.
2. [ ] Store credentials locally and on server storage (username + hash of password).
3. [ ] Store data in local files. Server stores messages until they have been received and acknowledged by receiving party.
4. [ ] Export chat conversations to a file and load on request.
5. [ ] Configuration file.
6. [ ] Event logging.
7. [ ] Unit testing.

## Milestone 4
1. [ ] Graceful shutdown and server system restore.
2. [ ] Detect client disconnections.
3. [ ] Client reconnect.
4. [ ] Retransmission of undelivered messages.

## Milestone 5
1. [ ] Rate limiting per client.
2. [ ] Session token with expiration.

## Milestone 6
1. [ ] Encryption in transit.
2. [ ] Encryption at rest and key storage.

## Milestone 7
1. [ ] Fuzz testing.
2. [ ] Integration testing.
3. [ ] Resilience testing.

## Milestone 8
1. [ ] Windows compatibility.
2. [ ] Stress testing.

## Milestone 9
1. [ ] System status reporting.
2. [ ] Monitoring.
2. [ ] Admin accounts.

## Milestone 10
1. [ ] File sending.
2. [ ] Group creation.

## Milestone 11
1. [ ] Split terminal window into message printing and user input.