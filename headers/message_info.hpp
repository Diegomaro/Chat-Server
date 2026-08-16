#include <atomic>
#include "constants/types.hpp"

struct MessageInfo{
    public:
        void resetMessage();
        bool valid_header{false};

        uint8_t type{types::INVALID_TYPE};
        uint32_t client_key{UINT32_MAX};
        uint64_t message_id{UINT64_MAX};
        uint32_t timestamp{UINT32_MAX};
        uint16_t payload_length{UINT16_MAX};
};