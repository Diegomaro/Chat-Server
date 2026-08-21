# Graceful Server Shutdown & Microservices

* Send error messages to all clients who have a pending message.
* Block new users to connect, send server shutdown status.
* Dual server services, one for auth and one for message processing.
  - Auth server sends registered client to server, server accepts if it it is not registered (or always accept it if I add multi device clients functionality)
