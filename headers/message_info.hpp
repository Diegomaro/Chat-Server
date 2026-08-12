#include <atomic>

class MessageInfo{
    private:
        uint16_t payload_length_{UINT16_MAX};
        uint8_t type_{0};
        uint32_t sender_key_{UINT32_MAX};
        uint32_t receiver_key_{UINT32_MAX};
};