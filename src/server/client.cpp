#include <arpa/inet.h>
#include <stdio.h>

#include "../../headers/client.hpp"

Client::Client(){
    memset(&name, 0, sizeof(name));
    memset(&ip, 0, sizeof(ip));

    for(uint32_t i = 0; i < config::BUFFER_SEGMENTS_PER_CLIENT; i++){
        buffer_pointers[i] = UINT32_MAX;
    }
}

void Client::resetMessage(){
    valid_header_ = false;
    payload_length = UINT16_MAX;
    type = types::INVALID_TYPE;
    receiver_key = UINT32_MAX;
    receiver_fd = -1;
}

uint32_t Client::getRemainingBytesWriting(){
    return buffer_pointers[writing_buffer] + config::BUFFER_SEGMENT_SIZE - writing_pointer;
}

uint32_t Client::getRemainingBytesReading(){
    return buffer_pointers[reading_buffer] + config::BUFFER_SEGMENT_SIZE - reading_pointer;
}

void Client::advanceReadingPointer(){
    if(reading_pointer + 1 < (buffer_pointers[reading_buffer] + config::BUFFER_SEGMENT_SIZE)){
        reading_pointer++;
    } else{
        reading_buffer++;
        if(reading_buffer >= config::BUFFER_SEGMENTS_PER_CLIENT){
            reading_buffer = 0;
        }
        reading_pointer = buffer_pointers[reading_buffer];
    }
}