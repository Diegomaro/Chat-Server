#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#include <iostream>
#include <fcntl.h>

#include <chrono>
#include <thread>

#include "../../headers/client_processor.hpp"

ClientProcessor::ClientProcessor(){
    memset(&hints_, 0, sizeof(hints_));
    hints_.ai_family = AF_UNSPEC;
    hints_.ai_socktype = SOCK_STREAM;

    client_socket_ = -1;
    epoll_fd_ = -1;

    incoming_buffer_ = new(std::nothrow) uint8_t[cts::READING_BUFFER_SIZE];
    outgoing_buffer_ = new(std::nothrow) uint8_t[cts::READING_BUFFER_SIZE];

    msg_len_ = 1024;

    starting_pointer_ = 0;
    writing_pointer_ = 0;
    reading_pointer_ = 0;

    byte_counter_ = 0;
    payload_length_ = UINT16_MAX;
    type_ = 0;
    sender_key_ = UINT32_MAX;
}

ClientProcessor::~ClientProcessor(){
    if(incoming_buffer_){
        delete [] incoming_buffer_;
        incoming_buffer_ = nullptr;
    }
    if(outgoing_buffer_){
        delete [] outgoing_buffer_;
        outgoing_buffer_ = nullptr;
    }
    close(client_socket_);
}

bool ClientProcessor::setupSocket(){
    int status;
    if((status = getaddrinfo("127.0.0.1", cts::SERVER_PORT, &hints_, &server_info_)) != 0){
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        return false;
    }
    if((client_socket_ = socket(server_info_->ai_family, server_info_->ai_socktype, server_info_->ai_protocol)) == -1){
        perror("socket failed");
        return false;
    }
    int yes = 1;
    if (setsockopt(client_socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt SO_REUSEADDR failed");
    }
    if(fcntl(client_socket_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return false;
    }
    if((connect(client_socket_, server_info_->ai_addr, server_info_->ai_addrlen)) == -1 && errno != EINPROGRESS){
        perror("connect failed");
        return false;
    }
    freeaddrinfo(server_info_);
    if ((epoll_fd_ = epoll_create1(0)) == -1) {
        perror("epoll failed");
        return false;
    }
    ev_.events = EPOLLIN | EPOLLET;
    ev_.data.fd = client_socket_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket_, &ev_);
    return true;
}

void ClientProcessor::centralLoop(){
    while(program_running_){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_fd_, events_, 10, 1000)) == -1){
            perror("epoll wait failed");
            return;
        }
        for (int i = 0; i < ready_polls; i++) {
            if(events_[i].data.fd == client_socket_){
                if (events_[i].events & EPOLLIN) {
                    bool receive_loop = true;
                    while(receive_loop){
                        int rcvf_state = receiveFromServer();
                        /*
                        Add receive state that means that the buffer is complete,
                        and requests a verify message before erasing the buffer and sending an error message
                        */
                        switch(rcvf_state){
                            case rts::SUCCESS:{
                                if(checkMessage() == rts::SUCCESS){
                                    if(!printMessage()){
                                        return;
                                    }
                                    if(!cleanIncomingBuffer()){
                                        return;
                                    }
                                    receive_loop = false;
                                }
                            //if missing timeout
                            } break;
                            case rts::NOTHING_TO_READ:{
                                receive_loop = false;
                            } break;
                            case rts::ERROR:{
                                return;
                            } break;
                            case rts::CLOSED_CONVERSATION:{
                                return;
                            } break;
                            case rts::INSUFFICIENT_BUFFER_SPACE:{
                                //send fail message to server
                                byte_counter_ = 0;
                                starting_pointer_ = 0;
                                reading_pointer_ = 0;
                                writing_pointer_ = 0;
                                sender_key_ = UINT32_MAX;
                                type_ = 0;
                                payload_length_ = UINT16_MAX;
                                receive_loop = false;
                            } break;
                        }
                    }
                }
            }
        }
        if(send_message_){
            {
                std::unique_lock<std::mutex> lock_message(read_mutex_);
                int ans = 0;
                switch(ans = sendMessage()){
                    case rts::SUCCESS:{
                        std::cout << "message sent correctly!" << std::endl;
                    }break;
                    case rts::NOTHING_TO_READ:{
                        std::cout << "Could not sent message!" << std::endl;
                    }break;
                    case rts::ERROR:{
                        std::cout << "Could not sent message!" << std::endl;
                    }break;
                }
                send_message_ = false;
            }
        }
        if(ready_polls == 0){
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

int ClientProcessor::sendMessage(){
    int bytes_to_send = msg_len_;
    int bytes_sent = 0;

    while(bytes_sent < bytes_to_send){
        int sent_bytes = 0;
        if((sent_bytes = send(
            client_socket_,
            &outgoing_buffer_[bytes_sent],
            (bytes_to_send - bytes_sent),
            0)) == -1)
        {
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return rts::NOTHING_TO_READ; // change to nothing to write
            } else{
                perror("Send of message failed.");
                return rts::ERROR;
            }
        } else{
            bytes_sent += sent_bytes;
        }
    }
    return rts::SUCCESS;
}

int ClientProcessor::receiveFromServer(){
    int total_bytes_received = 0;
    int bytes_received = 0;
    if(byte_counter_ + cts::BUFFER_READING_SIZE > cts::READING_BUFFER_SIZE){
        return rts::INSUFFICIENT_BUFFER_SPACE;
    }

    int reached_buffer_limit = false;
    int bytes_to_copy = cts::BUFFER_READING_SIZE;
    if(writing_pointer_ + cts::BUFFER_READING_SIZE > cts::READING_BUFFER_SIZE){
        bytes_to_copy = cts::READING_BUFFER_SIZE - writing_pointer_;
        reached_buffer_limit = true;
    }

    //byte counter
    while(total_bytes_received < bytes_to_copy){
        if((bytes_received = recv(
            client_socket_,
            &incoming_buffer_[writing_pointer_ + total_bytes_received],
            bytes_to_copy - total_bytes_received,
            0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                break;
            } else{
                perror("An error ocurred while receiving from client.");
                return rts::ERROR;
            }
        } else if(bytes_received == 0){
            break;
        }
        total_bytes_received += bytes_received;
    }
    if(total_bytes_received == 0){
        return rts::CLOSED_CONVERSATION;
    }

    byte_counter_ += total_bytes_received;
    writing_pointer_ = (writing_pointer_ + total_bytes_received) % cts::READING_BUFFER_SIZE;
    return rts::SUCCESS;
}

int ClientProcessor::checkMessage(){
    // HEAD_BITS
    if(byte_counter_ < 8){
        return rts::INVALID_MESSAGE;
    }
    reading_pointer_ = starting_pointer_;
    if((incoming_buffer_[reading_pointer_] ^ 0xFF) != 0){
        return rts::INVALID_MESSAGE;
    }
    advanceReadingPointer();
    // TYPE
    if(type_ == 0){
        type_ = incoming_buffer_[reading_pointer_];
    }
    advanceReadingPointer();

    // HOST_KEY
    if(sender_key_ == UINT32_MAX){
        sender_key_ = 0;
        for(int i = 0; i < 4; i++){
            sender_key_ = sender_key_ | (incoming_buffer_[reading_pointer_]) << ((cts::CLIENT_KEY_LENGTH - 1 - i) * 8);
            advanceReadingPointer();
        }
    } else{
        for(int i = 0; i < 4; i++){
            advanceReadingPointer();
        }
    }

    // PAYLOAD_LENGTH
    if(payload_length_ == UINT16_MAX){
        payload_length_ = 0;
        payload_length_ = incoming_buffer_[reading_pointer_] << 8;
        advanceReadingPointer();
        payload_length_ = payload_length_ | (incoming_buffer_[reading_pointer_]);
    } else{
        advanceReadingPointer();
    }
    advanceReadingPointer();

    switch(type_){
        case tys::USER:{
            if(payload_length_ == 0){
                return rts::INVALID_MESSAGE;
            }
            if(byte_counter_ < payload_length_ + cts::HEADER_SIZE){
                return rts::INCOMPLETE_MESSAGE;
            }
            return rts::SUCCESS;
        } break;
        case tys::GROUP:{
            //implement much later
        } break;
        case tys::AUTH_KEY:{
            // implement much later
        } break;
        case tys::SEND_REQUEST:{
            // implement much later
        } break;
        case tys::ACCEPT_REQUEST:{
            // implement much later
        } break;
        default:{
            return rts::INVALID_MESSAGE;
        }
    }
    return rts::SUCCESS;
}

void ClientProcessor::advanceReadingPointer(){
    if(reading_pointer_ + 1 >= cts::READING_BUFFER_SIZE){
        reading_pointer_ = 0;
    } else{
        reading_pointer_++;
    }
}

bool ClientProcessor::printMessage(){
    std::cout << static_cast<uint>(sender_key_) << ": ";
    for(int i = 0; i < payload_length_; i++){
        std::cout << static_cast<char>(incoming_buffer_[reading_pointer_]);
        advanceReadingPointer();
    }
    std::cout << std::endl << std::endl;
    return true;
}

bool ClientProcessor::cleanIncomingBuffer(){
    starting_pointer_ = reading_pointer_;
    byte_counter_ -= payload_length_ + cts::HEADER_SIZE;
    payload_length_ = UINT16_MAX;
    sender_key_ = UINT32_MAX;
    return true;
}


void ClientProcessor::messageInputLoop(){
    while(program_running_){
        int ans = 0;
        std::cout << "Menu." << std::endl
        << "1. Set message." << std::endl
        << "2. Set destinatory." << std::endl
        << "3. Send message." << std::endl
        << "4. Exit." << std::endl
        << ": ";
        std::cin >> ans;
        int result = 0;
        switch(ans){
            case 1:{
            result = setMessage();
            switch(result){
                case rts::SUCCESS:{
                    std::cout << "Message set correctly!" << std::endl;
                }break;
                case rts::INVALID_MESSAGE:{
                    std::cout << "Invalid message, please try again!" << std::endl;
                }break;
                case rts::ERROR:{
                    return;
                }
            }
            }break;
            case 2:{
                result = setDestinatory();
                switch(result){
                    case rts::SUCCESS:{
                        std::cout << "Receiver key set correctly!" << std::endl;
                    }break;
                    case rts::INVALID_MESSAGE:{
                        std::cout << "Invalid client, please try again!" << std::endl;
                    }break;
                    case rts::ERROR:{
                        return;
                    }
                }
            }break;
            case 3:{
                if(sender_key_ > 10 || message_.length() == 0 || message_.length() > cts::MAX_MESSAGE_SIZE){
                    std::cout << "Please set a valid receiver key and message first!" << std::endl;
                } else{
                    send_message_ = true;
                }
            }break;
            case 4:{
                program_running_ = false;
                return;
            }break;
            default:{
                std::cout << "Incorrect input" << std::endl;
            }

        }
        std::string temp_message;

    }
}

int ClientProcessor::setMessage(){
    if(!outgoing_buffer_){
        return rts::ERROR;
    }
    std::cout << "Message: ";
    std::getline(std::cin >> std::ws, message_);

    if(message_.length() == 0 || message_.length() > cts::MAX_MESSAGE_SIZE){
        return rts::INVALID_MESSAGE;
    }

    {
        std::unique_lock<std::mutex> lock_message(read_mutex_);
        uint16_t message_length = message_.length();

        outgoing_buffer_[0] = 255;
        outgoing_buffer_[1] = tys::USER;
        outgoing_buffer_[2] = sender_key_ >> 24; // remove these
        outgoing_buffer_[3] = sender_key_ >> 16;
        outgoing_buffer_[4] = sender_key_ >> 8;
        outgoing_buffer_[5] = sender_key_;
        outgoing_buffer_[6] = message_length >> 8;
        outgoing_buffer_[7] = message_length;


        for(int i = 0; i < message_length; i++){
            outgoing_buffer_[8 + i] = message_[i];
        }
        msg_len_ = 8 + message_length;
    }
    return rts::SUCCESS;
}

int ClientProcessor::setDestinatory(){
    if(!outgoing_buffer_){
        return rts::ERROR;
    }
    std::cout << "Destinatory (number between 1 and 10 [temporary]): ";
    std::cin >> sender_key_;

    if(sender_key_ > 10){
        return rts::INVALID_CLIENT;
    }
    outgoing_buffer_[2] = sender_key_ >> 24;
    outgoing_buffer_[3] = sender_key_ >> 16;
    outgoing_buffer_[4] = sender_key_ >> 8;
    outgoing_buffer_[5] = sender_key_;
    return rts::SUCCESS;
}