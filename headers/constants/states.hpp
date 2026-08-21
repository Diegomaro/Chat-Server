#pragma once

#include <cstdint>

namespace states{
    constexpr uint8_t NOT_SENT = 0;
    constexpr uint8_t SENT = 1;
    constexpr uint8_t PROCESSED = 2;
    constexpr uint8_t STORED = 3;
    constexpr uint8_t DELIVERED = 4;
    constexpr uint8_t ERROR = 5;
}
