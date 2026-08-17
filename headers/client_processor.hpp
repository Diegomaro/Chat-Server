#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>

#include<mutex>
#include <atomic>

#include "hash_table.hpp"
#include "constants/config.hpp"
#include "constants/status.hpp"
#include "constants/types.hpp"
#include "constants/info.hpp"
#include "constants/protocol.hpp"
#include "message_info.hpp"

#include <list>

class ClientProcessor{
    public:
        ClientProcessor();
        ~ClientProcessor();
        bool setupClientService();
        void centralLoop();
        void inputLoop();
    private:
        struct UsernameMapping{
            char username [protocol::USERNAME_LENGTH + 1] = {0};
            uint32_t key{UINT32_MAX};
            bool operator==(const UsernameMapping& other) const {
               return key == other.key;
            }
        };
        /*
        struct MessageMapping{
            MessageInfo msg_info;
            uint8_t *message{nullptr};

            bool operator==(const MessageMapping& other) const {
               return msg_info.message_id == other.msg_info.message_id;
            }
        };
        */

        // setup methods

        bool setupHashTables();
        bool setupBuffers();
        void setupHeaderTypes();
        void setupHeader(uint8_t *buffer, uint8_t type, uint8_t payload_length);
        bool setupSocket();

        // --- Central loop ---
        // outgoing data

        Status sendMessage(const uint8_t *buffer, int bytes_to_send);

        // incoming data

        Status receiveFromServer();
        Status messageProcessor();
        Status checkMessage();
        Status checkHeader();
        Status actOnMessage();

        // buffer managament

        void resetIncomingBuffer();
        void cleanIncomingBuffer();
        void advanceReadingPointer();

        // printing

        bool printMessage();


        // ---- Input loop ----

        bool welcomeInputLoop();
        bool messageInputLoop();

        // message input loop

        Status setMessage();
        Status setReceiver();

        // extra

        void updateLoggedInInfo();
        bool validateCredential(const std::string &credential, uint8_t min_length, uint8_t max_length);

        bool addUser(uint32_t key, const std::string &username);
        uint32_t getUserKey(const std::string &temp_username);
        char *getUserFromKey(uint32_t key);

        // helper methods

        void updateMessageID();
        void copyValueToBuffer(uint8_t *buffer, int position, int size, uint64_t value);
        int userNumericInput();
        bool integerCheck(const std::string &string, uint32_t length);
        uint32_t stringHash(const char *str);

        // attributes
        HashTable<UsernameMapping> username_to_key_;
        //HashTable<MessageMapping> pending_outgoing_messages_;
        std::list<UsernameMapping> incoming_requests_;
        //LinkedList<UsernameMapping> outgoing_requests_; // implement later

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

        uint8_t credentials_length_{0};

        uint32_t starting_pointer_{0};
        uint32_t writing_pointer_{0};
        uint32_t reading_pointer_{0};

        uint32_t byte_counter_{0};
        uint32_t receiver_key_{UINT32_MAX};

        uint32_t my_key_{UINT32_MAX};
        uint64_t my_message_id_{UINT64_MAX};
        uint32_t current_message_id{0}; // missing _

        MessageInfo msg_info;

        std::atomic<bool> logged_in_{false};

        std::atomic<bool> program_running_{true};
        std::atomic<bool> send_message_{false};
        std::atomic<bool> send_request_{false};
        std::atomic<bool> respond_request_{false};
        std::atomic<bool> send_register_{false};

        uint8_t auth_message_[protocol::HEADER_SIZE + protocol::USERNAME_LENGTH + protocol::MAX_PASSWORD_LENGTH];
        uint8_t ack_message_[protocol::HEADER_SIZE];
        uint8_t request_message_[protocol::HEADER_SIZE + protocol::USERNAME_LENGTH];
        uint8_t response_message_[protocol::HEADER_SIZE + protocol::USERNAME_LENGTH];
};