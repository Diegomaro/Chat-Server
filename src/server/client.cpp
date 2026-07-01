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
    starting_buffer_ = 0;
    writing_buffer_ = 0;
    buffer_pointers_amount_ = 0;
    byte_counter_ = 0;
    payload_length_ = UINT16_MAX;
    starting_pointer_ = 0;
    writing_pointer_ = 0;
    reading_pointer_ = 0;
    type_ = 0;
    receiver_key_ = UINT32_MAX;
    receiver_fd_ = -1;
    complete_message_ = false;
}

void Client::resetValues(){
    payload_length_ = UINT16_MAX;
    type_ = 0;
    receiver_key_ = 0;
    receiver_fd_ = -1;
    complete_message_ = false;
}

bool Client::advanceWritingPointer(){
    if(writing_pointer_ + 1 >= (buffer_pointers_[writing_buffer_] + Constants::MAX_MESSAGE_SIZE)){
        return false;
    } else{
        writing_pointer_++;
        return true;
    }
}

bool Client::advanceReadingPointer(){
    if(reading_pointer_ + 1 >= (buffer_pointers_[reading_buffer_] + Constants::MAX_MESSAGE_SIZE)){
        return false;
    } else{
        reading_pointer_++;
        return true;
    }
}