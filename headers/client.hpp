#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "constants.hpp"

class Client{
    public:
        Client();
        ~Client();

        //setup socket
        bool setupSocket();

        // data transmission
        bool receiveFromServer();
        bool sendMessage();

        // info on host and peer
        bool getHostName();
    private:
        struct addrinfo hints;
        struct addrinfo *server_info;

        int client_socket;

        char host_name [1024];

        const char* msg = "Sending something to server.";
        int msg_len;

        char msg_buffer [1024];

        struct sockaddr_storage server_addr;
        socklen_t server_addr_len;
};