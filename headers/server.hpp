#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>

#include "constants.hpp"

class Server{
    public:
        Server();
        ~Server();

        //setup listener socket
        bool setupListenerSocket();

        // connections to peers
        bool loopConnections();
        bool acceptConnection();
        bool closeConnection(int client_socket);

        // data transmission
        int receiveFromClient(int client_socket);
        bool printMessageFromClient();
        bool sendAcknowledgement(int client_socket);

        // info on host and peer
        bool getHostName();
        bool getPeerName();

    private:
        struct addrinfo hints;
        struct addrinfo *res;

        int listener_socket, epoll_socket;

        char host_name [1024];
        char peer_name [1024];

        int bytes_received;
        char msg_buffer [1024];

        int client_sockets [Constants::HOST_TOTAL];
        int current_client = 0;
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len;

        struct epoll_event ev;
        struct epoll_event events[20];
};