#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "constants.hpp"

class ClientProcessor{
    public:
        ClientProcessor();
        ~ClientProcessor();

        //setup socket
        bool setupSocket();

        // data transmission
        bool receiveFromServer();
        bool setMessage(std::string message);
        bool sendMessage();
    private:
        struct addrinfo hints;
        struct addrinfo *server_info;

        int client_socket;

        char msg [1024];
        int msg_len;

        char msg_buffer [1024];

        struct sockaddr_storage server_addr;
        socklen_t server_addr_len;
};