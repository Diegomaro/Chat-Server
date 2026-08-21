#pragma once

namespace info{
    constexpr uint8_t VALID_REGISTER = 1;
    constexpr uint8_t INVALID_CREDENTIAL = 2;
    constexpr uint8_t NOT_UNIQUE = 3;
    constexpr uint8_t ALREADY_LOGGED_IN = 4;
    constexpr uint8_t INVALID_PROTOCOL = 5;
    constexpr uint8_t INVALID_CLIENT = 6;
    constexpr uint8_t INVALID_MESSAGE = 7;
    constexpr uint8_t ALREADY_SENT_REQUEST = 8;
    constexpr uint8_t ALREADY_KNOWN_CLIENT = 9;
    constexpr uint8_t REQUEST_ALREADY_RECEIVED = 10;
    constexpr uint8_t UNAUTHENTICATED_USER = 11;
    constexpr uint8_t SEND_ERROR = 12;
    constexpr uint8_t COULD_NOT_REGISTER = 13;
}
