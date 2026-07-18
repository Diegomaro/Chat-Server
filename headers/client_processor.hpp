#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>

#include<mutex>
#include <atomic>

#include "hash_table.hpp"
#include "constants.hpp"

class ClientProcessor{
    public:
        ClientProcessor();
        ~ClientProcessor();

        struct UsernameMapping{
            char username_ [config::HOSTNAME_LENGTH];
            uint32_t key_;
        };
        unsigned long stringHash(char *str);

        //setup
        bool setupHeaderTypes();
        bool setupHashmap();
        bool setupSocket();

        void centralLoop();

        void inputLoop();
    private:
        // central loop
        int sendMessage(int bytes_to_send, uint8_t *buffer);

        int receiveFromServer();
        int checkMessage();
        int actOnMessage();
        void advanceReadingPointer();
        bool printMessage();
        bool cleanIncomingBuffer();

        //input loop
        bool welcomeInputLoop();
        bool validateCredential(std::string &credential, uint8_t min_length, uint8_t max_length);

        bool messageInputLoop();
        int setMessage();
        int setDestinatory();
        int manageRequests();

        bool addUser(uint32_t key, std::string username);
        uint32_t getUserKey(std::string temp_username);

        struct addrinfo hints_;
        struct addrinfo *server_info_;

        int client_socket_;
        int epoll_fd_;

        uint8_t *incoming_buffer_;
        uint8_t *outgoing_buffer_;

        std::string message_;
        int msg_len_;

        struct epoll_event ev_;
        struct epoll_event events_[10];

        uint32_t starting_pointer_;
        uint32_t writing_pointer_;
        uint32_t reading_pointer_;

        uint16_t byte_counter_; // missing checking what happens when exceeds 65k
        uint16_t payload_length_;
        uint8_t type_;
        uint32_t sender_key_;
        uint32_t receiver_key_;

        std::mutex read_mutex_;
        std::atomic<bool> program_running_{true};

        std::atomic<bool> send_message_{false};
        std::atomic<bool> send_request_{false};
        std::atomic<bool> send_register_{false};
        std::atomic<bool> logged_in_{false};

        uint32_t pending_messages;
        uint8_t auth_message_[config::HEADER_SIZE + config::HOSTNAME_LENGTH + config::MAX_PASSWORD_LENGTH];
        uint8_t ack_message_[config::HEADER_SIZE];
        uint8_t request_communication_[config::HEADER_SIZE + config::HOSTNAME_LENGTH];

        std::atomic<uint32_t> total_requests_{0}; // load from file later
        LinkedList<UsernameMapping> requests_;

        std::string username_;
        std::string password_;
        uint8_t credentials_length_;

        HashTable<UsernameMapping> username_to_key_;
        std::string receiving_username_;
};