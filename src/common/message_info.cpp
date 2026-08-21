#include <arpa/inet.h>
#include <stdio.h>

#include "../../headers/message_info.hpp"

void MessageInfo::resetMessage(){
    valid_header = false;

    type = types::INVALID_TYPE;
    client_key = UINT32_MAX;
    message_id = UINT64_MAX;
    timestamp = UINT32_MAX;
    payload_length = UINT16_MAX;
}
