#pragma once
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include "constants.hpp"

struct Client{
    public:
        Client();
        void resetMessage();

        uint32_t getRemainingBytesWriting();

        bool advanceReadingPointer();
        uint32_t getRemainingBytesReading();

        char name_ [config::HOSTNAME_LENGTH];
        char ip_ [INET6_ADDRSTRLEN];
        int port_;

        uint32_t buffer_pointers_[config::BUFFER_SEGMENTS_PER_CLIENT];
        uint8_t buffer_pointers_amount_;
        uint8_t starting_buffer_;
        uint8_t writing_buffer_;
        uint8_t reading_buffer_;

        uint32_t starting_pointer_;
        uint32_t writing_pointer_;
        uint32_t reading_pointer_;

        uint16_t byte_counter_; // missing checking what happens when exceeds 65k
        uint16_t payload_length_;
        uint8_t type_;
        uint32_t sender_key_;
        uint32_t receiver_key_;
        int receiver_fd_;

        bool logged_in_;
};