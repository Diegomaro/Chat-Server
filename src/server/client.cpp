#include <arpa/inet.h>
#include <stdio.h>

#include "../../headers/client.hpp"

Client::Client(){
    memset(&name_, 0, sizeof(name_));
    memset(&ip_, 0, sizeof(ip_));
    port_ = 0;

    for(int i = 0; i < cts::CLIENT_POINTERS; i++){
        buffer_pointers_[i] = UINT32_MAX;
    }
    buffer_pointers_amount_ = 0;
    starting_buffer_ = 0;
    writing_buffer_ = 0;
    reading_buffer_ = 0;

    starting_pointer_ = 0;
    writing_pointer_ = 0;
    reading_pointer_ = 0;

    byte_counter_ = 0;
    payload_length_ = UINT16_MAX;
    type_ = 0;
    sender_key_ = UINT32_MAX;
    receiver_key_ = UINT32_MAX;
    receiver_fd_ = -1;
}

void Client::resetMessage(){
    payload_length_ = UINT16_MAX;
    type_ = 0;
    receiver_key_ = UINT32_MAX;
    receiver_fd_ = -1;
}

uint32_t Client::getRemainingBytesWriting(){
    return buffer_pointers_[writing_buffer_] + cts::BUFFER_SEGMENT_SIZE - writing_pointer_;
}

bool Client::advanceReadingPointer(){
    if(reading_pointer_ + 1 >= (buffer_pointers_[reading_buffer_] + cts::BUFFER_SEGMENT_SIZE)){
        return false;
    } else{
        reading_pointer_++;
        return true;
    }
}

uint32_t Client::getRemainingBytesReading(){
    return buffer_pointers_[reading_buffer_] + cts::BUFFER_SEGMENT_SIZE - reading_pointer_;
}