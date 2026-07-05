#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>

#include<mutex>
#include <atomic>

#include "constants.hpp"

class ClientProcessor{
    public:
        ClientProcessor();
        ~ClientProcessor();

        //setup socket
        bool setupSocket();

        void centralLoop();

        void messageInputLoop();
    private:
        // central loop
        int sendMessage();
        int receiveFromServer();
        int checkMessage();
        void advanceReadingPointer();
        bool printMessage();
        bool cleanIncomingBuffer();

        // message input loop
        int setMessage();
        int setDestinatory();

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

        std::mutex read_mutex_;
        std::atomic<bool> program_running_{true};
        std::atomic<bool> send_message_{false};
};