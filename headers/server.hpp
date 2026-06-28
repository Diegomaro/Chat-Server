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

        //central loop
        bool loopConnections();

        // connections to client
        int acceptConnection();
        bool closeConnection(int client_socket);

        // data transmission
        int receiveFromClient(int client_socket);
        int sendAcknowledgement(int client_socket);

        // print data
        bool printClientInformation(int client_socket);
        bool printMessageFromClient();


    private:
        int accept_state;
        bool accept_loop;
        int rcvf_state;
        int sender_socket;
        bool receive_loop;
        int ack_state;

        struct addrinfo hints;
        struct addrinfo *res;

        char client_ip_buffer [INET_ADDRSTRLEN];
        struct sockaddr_in client_addr;
        unsigned int client_addr_length;

        int listener_socket;
        int client_sockets [Constants::HOST_TOTAL];
        int current_client = 0;

        int epoll_fd;

        char server_name [Constants::MAX_HOSTNAME_LENGTH];
        char client_name [Constants::MAX_HOSTNAME_LENGTH];

        struct sockaddr_storage client_sockaddr;
        socklen_t client_sockaddr_len;

        int bytes_received;
        char msg_buffer [1024];

        struct epoll_event ev;
        struct epoll_event events[Constants::MAX_EVENTS];
};