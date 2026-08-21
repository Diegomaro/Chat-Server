#pragma once

#include <cstdint>

namespace ack{
    constexpr uint8_t PROCESSED = 1;
    constexpr uint8_t STORED = 2;
    constexpr uint8_t DELIVERED = 3;
}
