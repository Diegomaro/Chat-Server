# Message Protocol (8 bytes)

## Strucutre
HEAD_BITS  TYPE     RECEIVER_KEY    PAYLOAD_LENGTH  PAYLOAD
1 byte     1 byte   4 bytes         2 bytes         0 - 65024 bytes

## Funcionality

### HEAD_BITS
Simple sequence of bits.

### TYPE
USER (1) | REGISTER (2) | LOGIN (3) | SEND_REQUEST (4) | ACCEPT_REQUEST (5) | REJECT_REQUEST(6) | INFO (7) | UPDATE (8) | ACK (9)
User - send message to another user.
Register - Send username + password to register.
Login - Send username + password to login.
Send request - Send request to another user to communicate.
Accept request - Accept a specific communication request.
Reject request - Reject a specific communication request.
INFO - Return status messages about messages.
ACK - Send ack of messages processed/delivered.

### RECEIVER_KEY
The receiver USER | The receiver GROUP | The sender USER | The sender GROUP
The username + key must exit in the database.
Marked as UINT_32_MAX for SEND_REQUEST, REGISTER and LOGIN.

### PAYLOAD_LENGTH
Length of payload in bytes.

### PAYLOAD
Payload format depends on the packet type.

## Payload Formats
USER payload
message bytes.

REGISTER payload
username - 16 bytes.
password - 8 to 60 bytes.

SEND_REQUEST payload
username - 16 bytes.

ACCEPT_REQUEST payload
username - 16 bytes.

REJECT_REQUEST payload
username - 16 bytes.

INFO payload
info status - 1 byte.

ACK payload
No payload.

UPDATE and LOGIN have not been implemented yet.