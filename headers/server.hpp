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

        //setup
        bool setupHashTables();
        bool setupBuffer();
        bool setupHeaderTypes();
        bool setupListenerSocket();

        //central loop
        bool loopConnections();

        // connections to client
        int acceptConnection();
        bool closeConnection(int client_socket);
        bool addClient();

        // data transmission
        int receiveFromClient(int client_socket);
        int checkMessage(int client_socket);
        int actOnMessage(int client_socket);
        bool cleanClientBuffer(int client_socket);
        bool advanceClientPointer(int client_socket);

        int sendAcknowledgement(int client_socket);
        int sendToClient(int client_socket);

        // print data
        bool printMessageFromClient(int client_socket);
        bool printClientInformation(int client_socket);

    private:
        struct addrinfo hints_;
        struct addrinfo *res_;

        int listener_socket_;
        int pending_client_;
        HashTable<Client> client_sockets_;
        HashTable<uint32_t> client_key_to_client_sockets_;

        int epoll_fd_;

        struct sockaddr_storage client_sockaddr_;
        socklen_t client_sockaddr_len_;

        Client *client_;

        int bytes_received_;
        uint8_t *buffer_pool_;
        LinkedList<uint32_t> available_buffers_;
        uint8_t *msg_buffer_;

        struct epoll_event ev_;
        struct epoll_event events_[config::MAX_EVENTS];

        uint32_t current_client_id_ = 0;
        // later on it will have to be non-volatile memory

        uint8_t ack_message_[config::HEADER_SIZE];
};