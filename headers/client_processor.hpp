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

        //setup
        bool setupClientService();
        //central
        void centralLoop();
        void inputLoop();
    private:
        struct UsernameMapping{
            char username [config::HOSTNAME_LENGTH];
            uint32_t key;
            bool operator==(const UsernameMapping& other) const {
               return key == other.key;
            }
        };

        // setup
        bool setupHashTables();
        bool setupHeaderTypes();
        bool setupSocket();

        // central loop
        // outgoing data
        int sendMessage(int bytes_to_send, uint8_t *buffer);

        // incoming data
        int receiveFromServer();
        int checkMessage();
        int actOnMessage();

        // buffer managament
        void cleanIncomingBuffer();
        void advanceReadingPointer();

        // printing
        bool printMessage();

        //input loop
        bool welcomeInputLoop();
        bool messageInputLoop();

        // message input loop
        int setMessage();
        int setReceiver();

        // helper methods
        bool validateCredential(const std::string &credential, uint8_t min_length, uint8_t max_length);

        bool addUser(uint32_t key, const std::string &username);
        uint32_t getUserKey(const std::string &temp_username);
        char *getUserFromKey(uint32_t key);

        unsigned long stringHash(const char *str);

        // attributes
        HashTable<UsernameMapping> username_to_key_;
        LinkedList<UsernameMapping> incoming_requests_;
        LinkedList<UsernameMapping> outgoing_requests_; // implement later

        uint8_t *incoming_buffer_{nullptr};
        uint8_t *outgoing_buffer_{nullptr};

        std::mutex read_mutex_;
        std::string message_;
        std::string username_;
        std::string password_;
        std::string receiving_username_;

        int client_socket_{-1};
        int epoll_fd_{-1};
        int msg_len_{-1};

        uint32_t pending_messages_{0};
        uint8_t credentials_length_{0};

        uint32_t starting_pointer_{0};
        uint32_t writing_pointer_{0};
        uint32_t reading_pointer_{0};

        uint16_t byte_counter_{0}; // missing checking what happens when exceeds 65k
        uint16_t payload_length_{UINT16_MAX};
        uint8_t type_{0};
        uint32_t sender_key_{UINT32_MAX};
        uint32_t receiver_key_{UINT32_MAX};

        std::atomic<bool> program_running_{true};
        std::atomic<bool> logged_in_{false};
        std::atomic<bool> send_message_{false};
        std::atomic<bool> send_request_{false};
        std::atomic<bool> respond_request_{false};
        std::atomic<bool> send_register_{false};

        struct epoll_event events_[config::MAX_EVENTS];

        uint8_t auth_message_[config::HEADER_SIZE + config::HOSTNAME_LENGTH + config::MAX_PASSWORD_LENGTH];
        uint8_t ack_message_[config::HEADER_SIZE];
        uint8_t request_communication_[config::HEADER_SIZE + config::HOSTNAME_LENGTH];
        uint8_t respond_communication_[config::HEADER_SIZE + config::HOSTNAME_LENGTH];
};