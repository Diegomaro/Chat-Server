#pragma once

#include <cstdint>
#include <chrono>

#include "protocol.hpp"

namespace config{

    // migrate to configuration file

    constexpr const char DEFAULT_IP[] = "127.0.0.1";
    constexpr const char DEFAULT_PORT[] = "60000";

    constexpr int MAX_EVENTS = 256;
    constexpr int PENDING_REQUESTS_MAX = 10;
    constexpr int MAX_INT_CHOICE_LENGTH = 4;

    // server user requirements

    constexpr int INITIAL_HASHTABLE_SIZE = 16;
    constexpr uint32_t MAX_HOSTS = 2048;
    constexpr uint8_t BUFFER_SEGMENTS_PER_CLIENT = 128;
    constexpr uint32_t BUFFER_SEGMENT_SIZE = 512;
    constexpr auto LOOP_TIMEOUT = std::chrono::milliseconds(100);

    // derived values

    constexpr uint32_t TOTAL_BUFFER_SEGMENTS =  MAX_HOSTS * BUFFER_SEGMENTS_PER_CLIENT;
    constexpr uint32_t BUFFER_SIZE = TOTAL_BUFFER_SEGMENTS * BUFFER_SEGMENT_SIZE;
    constexpr uint32_t READING_BUFFER_SIZE = BUFFER_SEGMENTS_PER_CLIENT * BUFFER_SEGMENT_SIZE;
    constexpr uint32_t MAX_MESSAGE_SIZE = READING_BUFFER_SIZE - BUFFER_SEGMENT_SIZE - protocol::HEADER_SIZE;
    constexpr uint32_t BUFFER_READING_SIZE = BUFFER_SEGMENT_SIZE * 16;
}