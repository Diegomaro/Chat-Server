#pragma once
#include <arpa/inet.h>
#include <cstdint>
#include <string.h>
#include "constants/config.hpp"
#include "constants/types.hpp"

struct Client{
    Client();
    void resetMessage();

    uint32_t getRemainingBytesWriting();
    uint32_t getRemainingBytesReading();

    void advanceReadingPointer();

    char name [config::HOSTNAME_LENGTH + 1] = {0};
    char ip [INET6_ADDRSTRLEN];
    int port{-1};

    uint32_t buffer_pointers[config::BUFFER_SEGMENTS_PER_CLIENT];
    uint8_t buffer_pointers_count{0};
    uint8_t starting_buffer{0};
    uint8_t writing_buffer{0};
    uint8_t reading_buffer{0};

    uint32_t starting_pointer{0};
    uint32_t writing_pointer{0};
    uint32_t reading_pointer{0};

    bool valid_header_{false};
    uint32_t byte_counter{0};
    uint16_t payload_length{UINT16_MAX};
    uint8_t type{types::INVALID_TYPE};
    uint32_t sender_key{UINT32_MAX};
    uint32_t receiver_key{UINT32_MAX};

    int receiver_fd{-1};

    bool logged_in{false};
};