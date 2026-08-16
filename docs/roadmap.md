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
1. [x] Intercommunication for little endian and big endian architectures.
2. [x] Improved protocol header.
3. [x] Protocol versioning.
4. [ ] Message ID system.
5. [ ] Reworked acknowledgement system to work with message IDs.
6. [ ] Duplicate message detection.
7. [ ] Timestamps.

## Milestone 3
1. [ ] Allow clients to login instead of registering.
2. [ ] Store credentials locally and on server storage (username +  password hash).
3. [ ] Store messages in local files.
4. [ ] Configuration file.
5. [ ] Event logging.
6. [ ] Export chat conversations to a file and load on request.
7. [ ] Unit testing.

## Milestone 4
1. [ ] Graceful shutdown and server system restore.
2. [ ] Detect client disconnections.
3. [ ] Client reconnect.
4. [ ] Deliver pending messages when clients reconnect.
5. [ ] Retransmission of undelivered messages.
6. [ ] Session token with expiration.

## Milestone 5
1. [ ] Encryption in transit.
2. [ ] Encryption at rest and key storage.

## Milestone 6
1. [ ] Fuzz testing.
2. [ ] Integration testing.
3. [ ] Resilience testing.

## Milestone 7
1. [ ] Windows compatibility.
2. [ ] Stress testing.

## Milestone 8
1. [ ] Server time synchronization with NTP.
2. [ ] Monitoring.
3. [ ] System status reporting.
4. [ ] Admin accounts.
5. [ ] Rate limiting per client.
6. [ ] Priority message queue.

## Milestone 9
1. [ ] File sending.
2. [ ] Group creation.

## Milestone 10
1. [ ] Split terminal window into message printing and user input.