#pragma once

#include <cstdint>

namespace protocol{
    // default values

    constexpr uint8_t USERNAME_LENGTH = 16;
    constexpr uint8_t MIN_PASSWORD_LENGTH = 8;
    constexpr uint8_t MAX_PASSWORD_LENGTH = 60;

    // fixed

    constexpr uint8_t CMP_VERSION = 2;

    constexpr uint8_t HEADER_SIZE = 20;
    constexpr uint8_t CLIENT_KEY_SIZE = 4;
    constexpr uint8_t MESSAGE_ID_SIZE = 8;
    constexpr uint8_t TIMESTAMP_SIZE = 4;
    constexpr uint8_t PAYLOAD_LENGTH_SIZE = 2;

    // derived values

    constexpr uint8_t AUTH_PAYLOAD_LENGTH = 1;
    constexpr uint8_t INFO_PAYLOAD_LENGTH = 1;

    constexpr uint8_t USERNAME_MESSAGE_LENGTH = HEADER_SIZE + USERNAME_LENGTH;
    constexpr uint8_t AUTH_MESSAGE_LENGTH = HEADER_SIZE + AUTH_PAYLOAD_LENGTH;
    constexpr uint8_t INFO_MESSAGE_LENGTH = HEADER_SIZE + INFO_PAYLOAD_LENGTH;


    namespace header{
        constexpr uint8_t HEAD_BITS_OFFSET = 0;
        constexpr uint8_t TYPE_OFFSET = 1;
        constexpr uint8_t CLIENT_KEY_OFFSET = 2;
        constexpr uint8_t MESSAGE_ID_OFFSET = 6;
        constexpr uint8_t TIMESTAMP_OFFSET = 14;
        constexpr uint8_t PAYLOAD_LENGTH_OFFSET = 18;
        constexpr uint8_t PAYLOAD_OFFSET = 20;
    }
}
