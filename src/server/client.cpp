#include <arpa/inet.h>
#include <stdio.h>

#include "../../headers/client.hpp"

Client::Client(){
    port_ = 0;
    memset(&name_, 0, sizeof(name_));
    memset(&ip_, 0, sizeof(ip_));
    for(int i = 0; i < Constants::CLIENT_POINTERS; i++){
        buffer_pointers_[i] = UINT32_MAX;
    }
    buffer_pointers_amount_ = 0;
    byte_counter_ = 0;
    payload_length_ = 0;
    starting_pointer_ = 0;
    writing_pointer_ = 0;
    type_ = 0;
    receiver_key_ = 0;
}