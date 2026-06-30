#pragma once
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include "constants.hpp"

struct Client{
    public:
        Client();
        void resetValues();
        bool advancePointer();
        char name_ [Constants::MAX_HOSTNAME_LENGTH];
        char ip_ [INET6_ADDRSTRLEN];
        int port_;
        uint32_t buffer_pointers_[Constants::CLIENT_POINTERS];
        uint8_t buffer_pointers_amount_;
        uint16_t byte_counter_; // max of 65536
        uint16_t payload_length_; // max of 65336. When to stop reading.
        uint32_t starting_pointer_; //if different from 0, loop around.
        uint32_t writing_pointer_; // where to write
        // Only read byte_counter bytes, otherwise random memory will be read
        uint8_t type_;
        uint16_t receiver_key_;
};