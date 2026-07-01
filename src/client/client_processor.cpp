#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#include <iostream>

#include "../../headers/client_processor.hpp"

ClientProcessor::ClientProcessor(){
    memset(&hints_, 0, sizeof(hints_));
    hints_.ai_family = AF_UNSPEC;
    hints_.ai_socktype = SOCK_STREAM;
    memset(&msg_buffer_, 0, sizeof(msg_buffer_));
    memset(&msg_, 0, sizeof(msg_));
    server_addr_len_ = sizeof(server_addr_);
    msg_len_ = 1024;
}

ClientProcessor::~ClientProcessor(){
    close(client_socket_);
}

bool ClientProcessor::setupSocket(){
    int status;
    if((status = getaddrinfo("127.0.0.1", Constants::SERVER_PORT, &hints_, &server_info_)) != 0){
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        return false;
    }
    if((client_socket_ = socket(server_info_->ai_family, server_info_->ai_socktype, server_info_->ai_protocol)) == -1){
        perror("socket failed");
        return false;
    }
    if((connect(client_socket_, server_info_->ai_addr, server_info_->ai_addrlen)) == -1){
        perror("connect failed");
        return false;
    }
    freeaddrinfo(server_info_);
    return true;
}

bool ClientProcessor::setMessage(std::string message){
    if(message.empty() || !msg_){
        return false;
    }
    uint16_t message_length = 0;
    if(message.length() > 1023){
        message_length = 1023;
    } else{
        message_length = message.length();
    }
    msg_[0] = 255; // 11111111
    msg_[1] = 1; // CLIENT type
    msg_[2] = 0; // receiver_key
    msg_[3] = 0; // receiver_key
    msg_[4] = 0; // receiver_key
    msg_[5] = 0; // receiver_key
    msg_[6] = message_length >> 8; // payload length
    msg_[7] = message_length; // payload length


    for(int i = 0; i < message_length; i++){
        msg_[8 + i] = message[i];
    }
    msg_len_ = 8 + message_length;
    return true;
}

bool ClientProcessor::sendMessage(){
    int bytes_sent = 0;
    if((bytes_sent = send(client_socket_, msg_, msg_len_, 0)) == -1){
        perror("send failed");
        return false;
    }
    return true;
}

bool ClientProcessor::receiveFromServer(){
    int bytes_received = 0;
    if((bytes_received = recv(client_socket_, msg_buffer_, sizeof(msg_buffer_) - 1, 0)) == -1){
        perror("received failed");
        return false;
    }
    if(bytes_received > 0){
        msg_buffer_[bytes_received] = '\0';
        std::cout << "Message received: " << msg_buffer_ << std::endl;
    }
    return true;
}