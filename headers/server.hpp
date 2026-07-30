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

        // setup
        bool setupServer();
        // central
        void centralLoop();
    private:
        struct UsernameMapping{
            char username[config::HOSTNAME_LENGTH];
            uint32_t key;
        };

        // setup
        bool setupHashTables();
        bool setupBuffer();
        bool setupHeaderTypes();
        bool setupListenerSocket();

        // connections to client
        int acceptConnection();
        bool addClient(const sockaddr_storage& client_sockaddr);
        bool closeConnection(int client_socket);

        // incoming data
        int receiveFromClient(int client_socket);
        int checkMessage(int client_socket);
        int actOnMessage(int client_socket);

        // client
        bool cleanClientBuffer(int client_socket);

        // sending data
        int sendStatusMessage(int client_socket, int receiver_fd, uint8_t *buffer, int bytes_to_send);
        int sendToClient(int client_socket);

        // print data
        bool printClientInformation(int client_socket);

        unsigned long stringHash(const char *str);

        HashTable<Client> clients_;
        HashTable<int> client_key_to_socket_;
        HashTable<UsernameMapping> username_to_client_key_;
        HashTable<LinkedList<uint32_t>*> client_key_to_known_keys_;

        uint8_t *buffer_pool_{nullptr};
        uint8_t *receiver_buffer_{nullptr};
        uint8_t *sending_buffer_{nullptr};
        LinkedList<uint32_t> available_buffers_;

        int epoll_fd_{-1};
        int listener_fd_{-1};
        int pending_client_fd_{-1};
        uint32_t next_client_key_{0};

        struct epoll_event events_[config::MAX_EVENTS];

        uint8_t processed_ack_message_[config::HEADER_SIZE];
        uint8_t delivered_ack_message_[config::HEADER_SIZE];
        uint8_t request_communication_message_[config::HEADER_SIZE + config::HOSTNAME_LENGTH];
        uint8_t accept_communication_message_[config::HEADER_SIZE + config::HOSTNAME_LENGTH];
        uint8_t authentication_message_[config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH];
};