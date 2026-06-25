#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "constants.hpp"

class Server{
    public:
        Server();
        ~Server();

        //setup listener socket
        bool setupListenerSocket();

        // connections to peers
        bool awaitConnection();
        bool closeConnection();

        // data transmission
        bool receiveFromPeer();
        bool sendAcknowledgement();

        // info on host and peer
        bool getHostName();
        bool getPeerName();

    private:
        struct addrinfo hints;
        struct addrinfo *res;

        int listener_socket;
        int peer_sockets [Constants::HOST_TOTAL];

        char host_name [1024];
        char peer_name [1024];

        char msg_buffer [1024];

        const char* ack_msg = "OK";
        int ack_msg_len;

        struct sockaddr_storage peer_addr;
        socklen_t peer_addr_len;
};