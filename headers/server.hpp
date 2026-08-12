#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>

#include "hash_table.hpp"
#include "client.hpp"

#include "constants/status.hpp"
#include "constants/info.hpp"
#include "constants/header.hpp"

#include <list>

class Server{
    public:
        Server();
        ~Server();
        bool setupServer();
        void centralLoop();
    private:
        struct UsernameMapping{
            char username[config::HOSTNAME_LENGTH + 1] = {0};
            uint32_t key{UINT32_MAX};
        };

        // Setup methods
        bool setupHashTables();
        void setupBuffers();
        void setupHeaderTypes();
        bool setupListenerSocket();

        // connections to client

        Status acceptConnection();
        Status addClient(const sockaddr_storage& client_sockaddr);
        Status closeConnection(int client_socket);

        // incoming data

        Status receiveFromClient(int client_socket);
        Status messageProcessor(int client_socket);
        Status checkMessage(int client_socket);
        Status checkHeader(int client_socket);
        Status actOnMessage(int client_socket);

        Status actOnUserMessage(int client_socket, Client *client);
        Status actOnRegister(int client_socket, Client *client);
        Status actOnSendRequest(Client *client);
        Status actOnRespondToRequest(int client_socket, Client *client);
        Status actOnAcknowledgement(Client *client);

        // sending data

        Status sendMessage(int receiver_fd, uint8_t *buffer, int bytes_to_send);
        Status sendToClient(int client_socket);

        // client
        Status resetClientBuffer(int client_socket);
        bool cleanClientBuffer(int client_socket);

        // print data

        bool printClientInformation(int client_socket);

        // helper method

        int stringHash(const char *str);

        // server attributes

        HashTable<Client> clients_;
        HashTable<int> client_key_to_socket_;
        HashTable<UsernameMapping> username_to_client_key_;
        HashTable<std::list<uint32_t>*> client_key_to_known_keys_;
        HashTable<std::list<uint32_t>*> client_key_to_requested_keys_;

        uint8_t *buffer_pool_{nullptr};
        uint8_t *receiver_buffer_{nullptr};
        uint8_t *sending_buffer_{nullptr};
        std::list<uint32_t> available_buffers_;

        int epoll_fd_{-1};
        int listener_fd_{-1};
        int pending_client_fd_{-1};
        uint32_t next_client_key_{0};

        struct epoll_event events_[config::MAX_EVENTS];

        uint8_t info_message_[config::INFO_MESSAGE_LENGTH];
        uint8_t processed_ack_message_[config::HEADER_SIZE];
        uint8_t delivered_ack_message_[config::HEADER_SIZE];
        uint8_t request_communication_message_[config::HEADER_SIZE + config::HOSTNAME_LENGTH];
        uint8_t accept_communication_message_[config::HEADER_SIZE + config::HOSTNAME_LENGTH];
};