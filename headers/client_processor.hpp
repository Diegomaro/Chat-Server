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
        struct addrinfo hints_;
        struct addrinfo *server_info_;

        int client_socket_;

        char msg_ [1024];
        int msg_len_;

        char msg_buffer_ [1024];

        struct sockaddr_storage server_addr_;
        socklen_t server_addr_len_;
};