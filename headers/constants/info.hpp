#pragma once

namespace info{
    constexpr uint8_t INVALID_CREDENTIAL = 1;
    constexpr uint8_t NOT_UNIQUE = 2;
    constexpr uint8_t ALREADY_LOGGED_IN = 3;
    constexpr uint8_t INVALID_PROTOCOL = 4;
    constexpr uint8_t INVALID_CLIENT = 5;
    constexpr uint8_t INVALID_MESSAGE = 6;
    constexpr uint8_t ALREADY_SENT_REQUEST = 7;
    constexpr uint8_t ALREADY_KNOWN_CLIENT = 8;
    constexpr uint8_t REQUEST_ALREADY_RECEIVED = 9;
    constexpr uint8_t UNAUTHENTICATED_USER = 10;
    constexpr uint8_t SEND_ERROR = 11;
    constexpr uint8_t COULD_NOT_REGISTER = 12;
}