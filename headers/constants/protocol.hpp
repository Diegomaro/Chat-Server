#pragma once

#include <cstdint>

namespace protocol{
    // default values

    constexpr uint8_t HOSTNAME_LENGTH = 16;
    constexpr uint8_t MIN_PASSWORD_LENGTH = 8;
    constexpr uint8_t MAX_PASSWORD_LENGTH = 60;

    // fixed

    constexpr uint8_t CLIENT_KEY_LENGTH = 4;
    constexpr uint8_t HEADER_SIZE = 8;

    // derived values
    constexpr uint8_t AUTH_MESSAGE_LENGTH = HEADER_SIZE + 1;
    constexpr uint8_t INFO_MESSAGE_LENGTH = HEADER_SIZE + 1;

    namespace header{
        constexpr uint8_t HEAD_BITS_OFFSET = 0;
        constexpr uint8_t TYPE_OFFSET = 1;
        constexpr uint8_t RECEIVER_KEY_OFFSET = 2;
        constexpr uint8_t PAYLOAD_LENGTH_OFFSET = 6;
        constexpr uint8_t PAYLOAD_OFFSET = 8;
    }
}
