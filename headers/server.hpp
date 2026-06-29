#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>

#include "hash_table.hpp"
#include "client.hpp"

class Server{
    public:
        Server();
        ~Server();

        bool setupHashTable();
        //setup listener socket
        bool setupListenerSocket();

        //central loop
        bool loopConnections();

        // connections to client
        int acceptConnection();
        bool closeConnection(int client_socket);

        bool addClient();

        // data transmission
        int receiveFromClient(int client_socket);
        int sendAcknowledgement(int client_socket);

        // print data
        bool printClientInformation(int client_socket);
        bool printMessageFromClient();

    private:
        int accept_state_;
        bool accept_loop_;
        int rcvf_state_;
        int sender_socket_;
        bool receive_loop_;
        int ack_state_;

        struct addrinfo hints_;
        struct addrinfo *res_;

        char client_ip_buffer_ [INET_ADDRSTRLEN];
        struct sockaddr_in client_addr_;
        unsigned int client_addr_length_;

        int listener_socket_;
        int pending_client_;
        HashTable<Client> client_sockets_;

        int epoll_fd_;

        char server_name_ [Constants::MAX_HOSTNAME_LENGTH];
        char client_name_ [Constants::MAX_HOSTNAME_LENGTH];

        struct sockaddr_storage client_sockaddr_;
        socklen_t client_sockaddr_len_;

        int bytes_received_;
        char msg_buffer_ [1024];

        struct epoll_event ev_;
        struct epoll_event events_[Constants::MAX_EVENTS];
};