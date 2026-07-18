#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include <cerrno>

#include <fcntl.h>

#include <cstdint>

#include <chrono>
#include <thread>

#include "../../headers/server.hpp"

Server::Server(){
    memset(&hints_, 0, sizeof(hints_));
    hints_.ai_family = AF_UNSPEC;
    hints_.ai_socktype = SOCK_STREAM;
    hints_.ai_flags = AI_PASSIVE;

    listener_socket_ = 0;
    pending_client_ = 0;

    epoll_fd_ = 0;

    client_sockaddr_len_ = sizeof(client_sockaddr_);

    client_ = nullptr;

    bytes_received_ = 0;
    buffer_pool_ = nullptr;
    msg_buffer_ = nullptr;

    current_client_key_ = 0; // later on, load from file

    memset(&processed_ack_message_, 0, sizeof(processed_ack_message_));
    memset(&delivered_ack_message_, 0, sizeof(delivered_ack_message_));
    memset(&request_communication_message_, 0, sizeof(request_communication_message_));
    memset(&accept_communication_message_, 0, sizeof(accept_communication_message_));
    memset(&authentication_message_, 0, sizeof(authentication_message_));
}

Server::~Server(){
    if(listener_socket_!= -1){
        close(listener_socket_);
    }
    if(epoll_fd_ != -1){
        close(epoll_fd_);
    }

    client_sockets_.resetNodeIndex();
    while(client_sockets_.hasNodes()){
        if(client_sockets_.hasNode()){
            int socket = (client_sockets_.getNode()->key_);
            if(socket != -1){
                std::cout << "closing socket " << socket << std::endl;
                close(socket);
            }
        }
        client_sockets_.advanceNode();
    }
    if(buffer_pool_){
        delete [] buffer_pool_;
        buffer_pool_ = nullptr;
    }
    if(msg_buffer_){
        delete [] msg_buffer_;
        msg_buffer_ = nullptr;
    }
}

bool Server::setupHashTables(){
    if(!client_sockets_.createTable(16)) {
        return false;
    }
    if(!client_key_to_client_sockets_.createTable(16)) {
        return false;
    }
    if(!client_name_to_client_key_.createTable(16)) {
        return false;
    }
    return true;
}

bool Server::setupBuffer(){
    if(buffer_pool_ || msg_buffer_){
        return false;
    }
    buffer_pool_ = new(std::nothrow) uint8_t [config::BUFFER_SIZE];
    if(!buffer_pool_){
        return false;
    }
    msg_buffer_ = new(std::nothrow) uint8_t [config::BUFFER_READING_SIZE];
    if(!msg_buffer_){
        return false;
    }

    uint32_t current_address = 0;
    for(int i = 0; i < config::AVAILABLE_BUFFER_SEGMENTS; i++){
        if(!available_buffers_.insertTail(current_address)){
            return false;
        }
        current_address += 0x00000200;
    }
    return true;
}

bool Server::setupHeaderTypes(){
    if(!processed_ack_message_ || !delivered_ack_message_){
        return false;
    }
    processed_ack_message_[0] = UINT8_MAX;
    processed_ack_message_[1] = types::ACK;
    processed_ack_message_[2] = UINT8_MAX;
    processed_ack_message_[3] = UINT8_MAX;
    processed_ack_message_[4] = UINT8_MAX;
    processed_ack_message_[5] = UINT8_MAX;
    processed_ack_message_[6] = 0;
    processed_ack_message_[7] = 0;

    delivered_ack_message_[0] = UINT8_MAX;
    delivered_ack_message_[1] = types::ACK;
    delivered_ack_message_[2] = UINT8_MAX;
    delivered_ack_message_[3] = UINT8_MAX;
    delivered_ack_message_[4] = UINT8_MAX;
    delivered_ack_message_[5] = UINT8_MAX;
    delivered_ack_message_[6] = 0;
    delivered_ack_message_[7] = 0;

    authentication_message_[0] = UINT8_MAX;
    authentication_message_[1] = types::REGISTER;
    authentication_message_[2] = UINT8_MAX;
    authentication_message_[3] = UINT8_MAX;
    authentication_message_[4] = UINT8_MAX;
    authentication_message_[5] = UINT8_MAX;
    authentication_message_[6] = 0;
    authentication_message_[7] = config::AUTH_PAYLOAD_LENGTH;
    authentication_message_[8] = 0;

    request_communication_message_[0] = UINT8_MAX;
    request_communication_message_[1] = types::SEND_REQUEST;
    request_communication_message_[2] = UINT8_MAX;
    request_communication_message_[3] = UINT8_MAX;
    request_communication_message_[4] = UINT8_MAX;
    request_communication_message_[5] = UINT8_MAX;
    request_communication_message_[6] = 0;
    request_communication_message_[7] = config::HOSTNAME_LENGTH;
    return true;
}

bool Server::setupListenerSocket(){
    int status = 0;
    if((status = getaddrinfo(NULL, config::SERVER_PORT, &hints_, &res_)) != 0){
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        return false;
    }
    if((listener_socket_ = socket(res_->ai_family, res_->ai_socktype, res_->ai_protocol)) == -1){
        perror("socket failed");
        return false;
    }
    int yes = 1;
    if (setsockopt(listener_socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt SO_REUSEADDR failed");
    }
    if((bind(listener_socket_, res_->ai_addr, res_->ai_addrlen)) == -1){
        perror("bind failed");
        return false;
    }
    if(fcntl(listener_socket_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return false;
    }
    freeaddrinfo(res_);
    if ((epoll_fd_ = epoll_create1(0)) == -1) {
        perror("epoll failed");
        return false;
    }
    ev_.events = EPOLLIN | EPOLLET;
    ev_.data.fd = listener_socket_;
    if(listen(listener_socket_, config::BACKLOG) == -1){
        perror("listen failed");
        return false;
    }
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listener_socket_, &ev_);
    return true;
}

bool Server::loopConnections(){
    while(true){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_fd_, events_, config::MAX_EVENTS, -1)) == -1){
            perror("epoll wait failed");
            return false;
        }
        for (int i = 0; i < ready_polls; i++) {
            if(events_[i].data.fd == listener_socket_){
                bool accept_loop = true;
                while(accept_loop){
                    uint8_t accept_state = acceptConnection();
                    switch(accept_state){
                        case status::SUCCESS:{
                            if(!printClientInformation(pending_client_)){
                                return false;
                            }
                        } break;
                        case status::NOTHING_TO_READ:{
                            accept_loop = false;
                        } break;
                        case status::ERROR:{
                            return false;
                        } break;
                        case status::EXCEEDED_CLIENT_MAX:{
                            return false;
                        } break;
                    }
                }
            } else if (events_[i].events & EPOLLIN) {
                int sender_socket = events_[i].data.fd;
                bool receive_loop = true;
                while(receive_loop){
                    int rcvf_state = receiveFromClient(sender_socket);
                    switch(rcvf_state){
                        case status::SUCCESS:{
                            int check_state = checkMessage(sender_socket);
                            switch(check_state){
                                case status::SUCCESS:{
                                    actOnMessage(sender_socket);
                                     if(!cleanClientBuffer(sender_socket)){
                                       return status::ERROR;
                                    }
                                    // cannot send messages until authenticated
                                }break;
                                case status::ERROR:{
                                    return false;
                                }break;
                                case status::INVALID_MESSAGE:{
                                    //send signal of error to user
                                    receive_loop = false;
                                }break;
                                case status::INVALID_CLIENT:{
                                    //send signal of error to user
                                    receive_loop = false;
                                }break;
                            }
                           //if missing timeout
                        } break;
                        case status::NOTHING_TO_READ:{
                            receive_loop = false;
                        } break;
                        case status::INVALID_CLIENT:{
                            return false;
                        }break;
                        case status::CLOSED_CONVERSATION:{
                            if(!closeConnection(sender_socket)){
                                return false;
                            }
                            receive_loop = false;
                            return true; // to test for memory leaks
                        } break;
                        case status::ERROR:{
                            return false;
                        } break;
                        case status::EXCEEDED_CLIENT_BUFFER_SIZE:{
                            // return error message to client and restart buffer segments
                        } break;
                    }
                }
            }
        }
        if(ready_polls == 0){
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return true;
}

// returns EXCEEDED_CLIENT_MAX, NOTHING_TO_READ, PERROR, SUCCESS
int Server::acceptConnection(){
    if(client_sockets_.getDataCount() + 1 >= config::MAX_HOSTS){
        return status::EXCEEDED_CLIENT_MAX;
    }
    pending_client_ = 0;
    if((pending_client_ = accept(listener_socket_, (struct sockaddr *)&client_sockaddr_, &client_sockaddr_len_)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return status::NOTHING_TO_READ;
        } else{
            perror("accept failed");
            return status::ERROR;
        }
    }
    if(!addClient()){
        return status::EXCEEDED_CLIENT_MAX;
    }
    if(fcntl(pending_client_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return status::ERROR;
    }
    ev_.data.fd = pending_client_;
    ev_.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pending_client_, &ev_) == -1){
        perror("epoll failed");
        return status::ERROR;
    }
    return status::SUCCESS;
}

bool Server::closeConnection(int client_socket){
    if(close(client_socket) == -1){
        perror("clossing failed");
        return false;
    }
    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return false;
    }
    int8_t buffers_erased = 0;
    for(int i = 0; i < config::BUFFER_SEGMENTS_PER_CLIENT; i++){
        if(buffers_erased >= client_->buffer_pointers_amount_){
            break;
        }
        if(client_->buffer_pointers_[i] != UINT32_MAX){
            if(!available_buffers_.insertHead(client_->buffer_pointers_[i])){
                return false;
            }
            buffers_erased++;
        }
    }
    if(!client_key_to_client_sockets_.deleteNode(client_->sender_key_)){
        return false;
    }
    if(!client_name_to_client_key_.deleteNode(stringHash(client_->name_))){
        return false;
    }
    if(!client_sockets_.deleteNode(client_socket)){
        return false;
    }
    std::cout << "manual close of socket "  << client_socket << std::endl;
    client_ = nullptr;
    return true;
}

bool Server::addClient(){
    Client new_client;
    new_client.name_[0] = '\0';
    void* addr;
    if(client_sockaddr_.ss_family == AF_INET) {
        sockaddr_in* ipv4 = (sockaddr_in*)&client_sockaddr_;
        addr = &(ipv4->sin_addr);
        new_client.port_ = ntohs(ipv4->sin_port);
    }
    else {
        sockaddr_in6* ipv6 = (sockaddr_in6*)&client_sockaddr_;
        addr = &(ipv6->sin6_addr);
        new_client.port_ = ntohs(ipv6->sin6_port);
    }
    inet_ntop(client_sockaddr_.ss_family, addr, new_client.ip_, sizeof(new_client.ip_));
    if(available_buffers_.isEmpty()){
        return false;
    }
    new_client.buffer_pointers_[0] = available_buffers_.getHead();
    if(!available_buffers_.deleteHead()){
        return false;
    }
    new_client.buffer_pointers_amount_ = 1;
    new_client.starting_pointer_ = new_client.buffer_pointers_[0];
    new_client.reading_pointer_ = new_client.buffer_pointers_[0];
    new_client.writing_pointer_ = new_client.buffer_pointers_[0];

    if(!client_sockets_.insertNode(pending_client_, new_client)){
        return false;
    }
    return true;
}

// returns INVALID_CLIENT, ERROR, NOTHING_TO_READ, CLOSED_CONVERSATION, SUCCESS, EXCEEDED_CLIENT_BUFFER_SIZE, INSUFFICIENT_BUFFER_SPACE
int Server::receiveFromClient(int client_socket){
    if(client_socket == -1){
        return status::INVALID_CLIENT;
    }

    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return status::ERROR;
    }
    msg_buffer_[0] = '\0';
    if((bytes_received_ = recv(client_socket, msg_buffer_, config::BUFFER_READING_SIZE, 0)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return status::NOTHING_TO_READ;
        } else{
            perror("An error ocurred while receiving from client.");
            return status::ERROR;
        }
    }
    if(bytes_received_ == 0){
        return status::CLOSED_CONVERSATION;
    }

    int bytes_remaining = bytes_received_;
    int msg_buffer_offset = 0;
    while(bytes_remaining > 0){
        uint32_t available_segment_bytes = client_->getRemainingBytesWriting();
        if(available_segment_bytes > bytes_remaining){
            memcpy(&buffer_pool_[client_->writing_pointer_], &msg_buffer_[msg_buffer_offset], bytes_remaining);
            client_->writing_pointer_ += bytes_remaining;
            client_->byte_counter_ += bytes_remaining;
            bytes_remaining = 0;
        }else {
            memcpy(&buffer_pool_[client_->writing_pointer_], &msg_buffer_[msg_buffer_offset], available_segment_bytes);
            client_->byte_counter_ += available_segment_bytes;
            msg_buffer_offset += available_segment_bytes;
            bytes_remaining -= available_segment_bytes;
            if(client_->buffer_pointers_amount_ + 1 >= config::BUFFER_SEGMENTS_PER_CLIENT){
                if(checkMessage(client_socket) == status::SUCCESS){
                    std::cout << "work on it later" << std::endl;
                    // delete old message
                }
                return status::EXCEEDED_CLIENT_BUFFER_SIZE;
            } else if(available_buffers_.isEmpty()){
                return status::INSUFFICIENT_BUFFER_SPACE;
            } else{
                uint32_t new_buffer_segment = available_buffers_.getHead();
                client_->buffer_pointers_[(client_->writing_buffer_ + 1) % 128] = new_buffer_segment;
                if(!available_buffers_.deleteHead()){
                    return status::ERROR;
                }
                client_->buffer_pointers_amount_++;
                client_->writing_buffer_ = (client_->writing_buffer_ + 1) % 128;
                client_->writing_pointer_ = new_buffer_segment;
            }
        }
    }
    client_ = nullptr;
    return status::SUCCESS;
}

/*
Verifies that a message has a valid header and replaces target key with sender key.
returns SUCCESS, ERROR, INVALID_MESSAGE, INVALID_CLIENT, INCOMPLETE_MESSAGE
*/
int Server::checkMessage(int client_socket){
    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return status::ERROR;
    }
    if(client_->byte_counter_ < 8){
        return status::INVALID_MESSAGE;
    }
    // HEAD_BITS
    if((buffer_pool_[client_->reading_pointer_] ^ 0xFF) != 0){
        return status::INVALID_MESSAGE;
    }
    if(!advanceClientPointer(client_socket)){
        return status::INVALID_MESSAGE; // other error
    }

    // TYPE
    if(client_->type_ == 0){
        client_->type_ = buffer_pool_[client_->reading_pointer_];
    }
    if(!advanceClientPointer(client_socket)){
        return status::INVALID_MESSAGE;
    }

    // HOST_KEY
    if(client_->receiver_key_ == UINT32_MAX){
        uint32_t tmp_pointer = client_->reading_pointer_;
        uint8_t tmp_buffer = client_->reading_buffer_;

        client_->receiver_key_ = 0;

        for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
            client_->receiver_key_ += (buffer_pool_[client_->reading_pointer_]) << ((config::CLIENT_KEY_LENGTH - 1 - i) * 8);
            if(!advanceClientPointer(client_socket)){
                return status::INVALID_MESSAGE;
            }
        }
        if(client_->receiver_key_ != UINT32_MAX){
            // repopulate the hash table each time the server opens
            if(!client_key_to_client_sockets_.searchNode(client_->receiver_key_)){
                return status::INVALID_CLIENT;
            }
            client_->receiver_fd_ = *client_key_to_client_sockets_.getNode(client_->receiver_key_);

            client_->reading_pointer_ = tmp_pointer;
            client_->reading_buffer_ = tmp_buffer;

            for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
                buffer_pool_[client_->reading_pointer_] = client_->sender_key_ << ((config::CLIENT_KEY_LENGTH - 1 - i) * 8);
                if(!advanceClientPointer(client_socket)){
                    return status::INVALID_MESSAGE;
                }
            }
        }
    } else{
        for(int i = 0; i < 4; i++){
            if(!advanceClientPointer(client_socket)){
                return status::INVALID_MESSAGE;
            }
        }
    }

    // PAYLOAD_LENGTH
    if(client_->payload_length_ == UINT16_MAX){
        client_->payload_length_ = 0;
        client_->payload_length_ = buffer_pool_[client_->reading_pointer_] << 8;
        if(!advanceClientPointer(client_socket)){
            return status::INVALID_MESSAGE;
        }
        client_->payload_length_ = client_->payload_length_ | (buffer_pool_[client_->reading_pointer_]);
    } else{
        if(!advanceClientPointer(client_socket)){
            return status::INVALID_MESSAGE;
        }
    }
    if(!advanceClientPointer(client_socket)){
        return status::INVALID_MESSAGE;
    }
    if(client_->byte_counter_ < client_->payload_length_ + config::HEADER_SIZE){
        return status::INCOMPLETE_MESSAGE;
    }
    return status::SUCCESS;
}

/*
Depending on the type of message received, it does something different.
returns SUCCESS, ERROR, INVALID_MESSAGE, RESOURCE_UNAVAILABLE, INVALID_CLIENT, INCOMPLETE_MESSAGE
*/
int Server::actOnMessage(int client_socket){
    client_ = client_sockets_.getNode(client_socket);
    switch(client_->type_){
        case types::USER:{
            if(client_->payload_length_ == 0 || client_->payload_length_ > config::MAX_MESSAGE_SIZE){
                return status::INVALID_MESSAGE;
            }
            if(!client_sockets_.searchNode(client_->receiver_fd_)){
                return status::INVALID_CLIENT;
                // later it should be changed to store all client keys, regardless of whether online or not.
                // If client is not available it should be stored in some file. (much later)
            }
            uint8_t ack_state = sendProcessedAcknowledgement(client_socket);
            uint8_t send_state = sendToClient(client_socket);

            switch(ack_state){
                case status::RESOURCE_UNAVAILABLE:{
                    // should not return, rather be stored
                    return status::RESOURCE_UNAVAILABLE;
                } break;
                case status::ERROR:{
                    return status::ERROR;
                } break;
            }
            //if(!printMessageFromClient(client_socket)) return false;
            /*
            verify if target is available. If not available, store the message in txt file.
            Otherwise request sending buffer.
            */
            switch(send_state){
                case status::RESOURCE_UNAVAILABLE:{
                    //should not return, rather be stored
                    return status::RESOURCE_UNAVAILABLE;
                } break;
                case status::ERROR:{
                    return status::ERROR;
                } break;
            }
            return status::SUCCESS;
        } break;
        case types::REGISTER:{
            if(client_->logged_in_){
                // do smth
            }

            if(client_->payload_length_ < config::HOSTNAME_LENGTH + config::MIN_PASSWORD_LENGTH
            || client_->payload_length_ > config::HOSTNAME_LENGTH + config::MAX_PASSWORD_LENGTH){
                return status::INVALID_MESSAGE;
            }

            //CHECK CREDENTIALS
            uint8_t username [config::HOSTNAME_LENGTH];
            uint32_t usr_ctr = 0;
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                username[i] = buffer_pool_[client_->reading_pointer_];
                if((username[i] > 0 && username[i] < 48)
                || (username[i] > 57 && username[i] < 65)
                || (username[i] > 90 && username[i] < 95)
                || (username[i] > 95 && username[i] < 97)
                || username[i] > 122){
                    return sendAuthentication(client_socket, auth::INVALID_CREDENTIAL);
                }
                usr_ctr++;
                if(!advanceClientPointer(client_socket)){
                    return status::INVALID_MESSAGE;
                }
            }
            if(usr_ctr < 1){
                return sendAuthentication(client_socket, auth::INVALID_CREDENTIAL);
            }
            uint8_t password [client_->payload_length_ - config::HOSTNAME_LENGTH];
            uint32_t psw_ctr = 0;
            for(int i = 0; i < client_->payload_length_ - config::HOSTNAME_LENGTH; i++){
                password[i] = buffer_pool_[client_->reading_pointer_];
                if(password[i] < 48
                || (password[i] > 57 && password[i] < 65)
                || (password[i] > 90 && password[i] < 95)
                || (password[i] > 95 && password[i] < 97)
                || password[i] > 122){
                    return sendAuthentication(client_socket, auth::INVALID_CREDENTIAL);
                }
                psw_ctr++;
                if(!advanceClientPointer(client_socket)){
                    return status::INVALID_MESSAGE;
                }
            }


            if(psw_ctr < config::MIN_PASSWORD_LENGTH || psw_ctr > config::MAX_PASSWORD_LENGTH){
                return sendAuthentication(client_socket, auth::INVALID_CREDENTIAL);
            }

            // unique username
            if(client_name_to_client_key_.getDataCount() != 0){
                client_name_to_client_key_.resetNodeIndex();
                while(client_name_to_client_key_.hasNodes()){
                    if(client_name_to_client_key_.hasNode()){
                        bool equal_usernames = true;
                        char *ref_username = client_name_to_client_key_.getNode()->data_.username_;
                        for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                            if(ref_username[i] != username[i]){
                                equal_usernames = false;
                                break;
                            }
                        }
                        if(equal_usernames){
                            return sendAuthentication(client_socket, auth::NOT_UNIQUE);
                        }
                    }
                    client_name_to_client_key_.advanceNode();
                }
            }
            // get key
            client_->sender_key_ = current_client_key_;
            if(current_client_key_ >= UINT32_MAX){
                return status::EXCEEDED_CLIENT_MAX;
            }
            current_client_key_++;
            UsernameMapping userMapping;
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                client_->name_[i] = username[i];
                userMapping.username_[i] = username[i];
            }
            userMapping.key_ = client_->sender_key_;

            if(!client_name_to_client_key_.insertNode(stringHash(client_->name_), userMapping)){
                return status::ERROR;
            }
            if(!client_key_to_client_sockets_.insertNode(client_->sender_key_, client_socket)){
                return status::ERROR;
            }
            client_->logged_in_ = true;
            return sendAuthentication(client_socket, auth::VALID);
        } break;
        case types::LOGIN:{
            // implement eventually
        } break;
        case types::SEND_REQUEST:{
            // search username. Verify that the connection is not established, if yes, just return true without doing anything.
            if(client_->payload_length_ != config::HOSTNAME_LENGTH){
                return status::INVALID_MESSAGE;
            }
            uint8_t target_username [config::HOSTNAME_LENGTH];
            uint32_t usr_ctr = 0;
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                target_username[i] = buffer_pool_[client_->reading_pointer_];
                if((target_username[i] > 0 && target_username[i] < 48)
                || (target_username[i] > 57 && target_username[i] < 65)
                || (target_username[i] > 90 && target_username[i] < 95)
                || (target_username[i] > 95 && target_username[i] < 97)
                || target_username[i] > 122){
                    return status::INVALID_MESSAGE;
                }
                usr_ctr++;
                if(!advanceClientPointer(client_socket)){
                    return status::INVALID_MESSAGE;
                }
            }
            if(usr_ctr < 1){
                return status::INVALID_CLIENT;
            }
            if(client_name_to_client_key_.getDataCount() == 0){
                return status::INVALID_CLIENT;
            }
            char *client_username;
            uint32_t client_key;
            client_name_to_client_key_.resetNodeIndex();
            while(client_name_to_client_key_.hasNodes()){
                if(client_name_to_client_key_.hasNode()){
                    bool equal_usernames = true;
                    client_username = client_name_to_client_key_.getNode()->data_.username_;
                    client_key = client_name_to_client_key_.getNode()->data_.key_;
                    client_->receiver_key_ = client_key;
                    for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                        if(client_username[i] != target_username[i]){
                            equal_usernames = false;
                            break;
                        }
                    }
                    if(equal_usernames){
                        int *client_fd = client_key_to_client_sockets_.getNode(client_key);
                        if(client_fd == nullptr){
                            return status::ERROR;
                        }
                        client_->receiver_fd_ = *client_fd;
                        for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
                            request_communication_message_[i + 2] = client_->sender_key_ << ((config::CLIENT_KEY_LENGTH - 1 - i) * 8);
                        }
                        for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                            request_communication_message_[i + config::HEADER_SIZE] = client_->name_[i];
                        }
                        return sendRequestCommunication(client_socket);
                    }
                }
                client_name_to_client_key_.advanceNode();
            }
            return status::INVALID_CLIENT;

        } break;
        case types::RESPOND_TO_REQUEST:{
        } break;
        case types::ACK:{
            if(!client_sockets_.searchNode(client_->receiver_fd_)){
                return status::INVALID_CLIENT;
            }
            uint8_t ack_state = sendDeliveredAcknowledgement(client_socket);
            switch(ack_state){
                case status::RESOURCE_UNAVAILABLE:{
                    // should not return, rather be stored
                    return status::RESOURCE_UNAVAILABLE;
                } break;
                case status::ERROR:{
                    return status::ERROR;
                } break;
            }
            return status::SUCCESS;

        } break;
        default:{
            return status::INVALID_MESSAGE;
        }
    }
    return status::SUCCESS;
}

bool Server::cleanClientBuffer(int client_socket){
    int dif = client_->writing_buffer_ - client_->starting_buffer_;
    if(dif < 0){
        dif *= -1;
    }
    for(int i = 0; i < dif; i++){
        if(!available_buffers_.insertHead(client_->buffer_pointers_[(client_->starting_buffer_ + i) % 128])){
            return false;
        }
        client_->buffer_pointers_[(client_->starting_buffer_ + i) % 128] = UINT32_MAX;
        client_->buffer_pointers_amount_--;
    }
    client_->starting_buffer_ = client_->writing_buffer_;
    client_->reading_buffer_ = client_->writing_buffer_;

    client_->starting_pointer_ = client_->reading_pointer_;
    client_->byte_counter_ -= client_->payload_length_ + config::HEADER_SIZE;
    client_->resetMessage();
    return true;
}

bool Server::advanceClientPointer(int client_socket){
    if(!client_->advanceReadingPointer()){
        client_->reading_buffer_++;
        if(client_->reading_buffer_ >= config::BUFFER_SEGMENTS_PER_CLIENT){
            return false;
        } else{
            client_->reading_pointer_ = client_->buffer_pointers_[client_->reading_buffer_];
        }
    }
    return true;
}

/*
Sends acknowledgement to client when the entire message has been processed and verified.
Return values: SUCCESS, ERROR, RESOURCE_UNAVAILABLE.
*/
int Server::sendProcessedAcknowledgement(int client_socket){
    int total_bytes_sent = 0;
    int bytes_sent = 0;
    while(total_bytes_sent < config::HEADER_SIZE){
        if((bytes_sent = send(client_socket, &processed_ack_message_[total_bytes_sent], config::HEADER_SIZE - total_bytes_sent, 0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of acknowledgement failed.");
                return status::ERROR;
            }
        }
        total_bytes_sent += bytes_sent;
    }
    return status::SUCCESS;
}

/*
Sends acknowledgement to client when the destinatory client has received the entire message.
Return values: SUCCESS, ERROR, RESOURCE_UNAVAILABLE.
*/
int Server::sendDeliveredAcknowledgement(int client_socket){
    client_ = client_sockets_.getNode(client_socket);

    int total_bytes_sent = 0;
    int bytes_sent = 0;
    delivered_ack_message_[2] = client_->sender_key_ >> 24;
    delivered_ack_message_[3] = client_->sender_key_ >> 16;
    delivered_ack_message_[4] = client_->sender_key_ >> 8;
    delivered_ack_message_[5] = client_->sender_key_;


    while(total_bytes_sent < config::HEADER_SIZE){
        if((bytes_sent = send(client_->receiver_fd_, &delivered_ack_message_[total_bytes_sent], config::HEADER_SIZE - total_bytes_sent, 0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of acknowledgement failed.");
                return status::ERROR;
            }
        }
        total_bytes_sent += bytes_sent;
    }
    return status::SUCCESS;
}

/*
Sends authentication results to client.
Return values: SUCCESS, ERROR, RESOURCE_UNAVAILABLE.
*/
int Server::sendAuthentication(int client_socket, u_int8_t auth){
    client_ = client_sockets_.getNode(client_socket);

    int total_bytes_sent = 0;
    int bytes_sent = 0;
    authentication_message_[8] = auth;

    while(total_bytes_sent < config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH){
        if((bytes_sent = send(client_socket, &authentication_message_[total_bytes_sent], config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH - total_bytes_sent, 0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of authentication failed.");
                return status::ERROR;
            }
        }
        total_bytes_sent += bytes_sent;
    }
    return status::SUCCESS;
}

/*
Sends request results to client.
Return values: SUCCESS, ERROR, RESOURCE_UNAVAILABLE.
*/
int Server::sendRequestCommunication(int client_socket){
    client_ = client_sockets_.getNode(client_socket);

    int total_bytes_sent = 0;
    int bytes_sent = 0;

    while(total_bytes_sent < config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH){
        if((bytes_sent = send(client_->receiver_fd_, &request_communication_message_[total_bytes_sent], config::HEADER_SIZE + config::HOSTNAME_LENGTH - total_bytes_sent, 0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of authentication failed.");
                return status::ERROR;
            }
        }
        total_bytes_sent += bytes_sent;
    }
    std::cout << "fd: " << client_->receiver_fd_ << std::endl;
    return status::SUCCESS;
}

/*
Sends message from one client to another.
Return values: SUCCESS, ERROR, RESOURCE_UNAVAILABLE.
*/
int Server::sendToClient(int client_socket){
    client_ = client_sockets_.getNode(client_socket);

    int print_pointer = config::READER_BUFFER_POINTER;
    int bytes_to_send = client_->payload_length_ + config::HEADER_SIZE;
    int total_bytes_sent = 0;

    client_->reading_pointer_ = client_->starting_pointer_;
    for(int i = 0; i <  bytes_to_send; i++){
        buffer_pool_[print_pointer] = buffer_pool_[client_->reading_pointer_];
        if(!advanceClientPointer(client_socket)){
            return status::ERROR;
        }
        print_pointer++;
    }
    while(total_bytes_sent < bytes_to_send){
        int sent_bytes = 0;
        if((sent_bytes = send(
            client_->receiver_fd_,
            &buffer_pool_[config::READER_BUFFER_POINTER + total_bytes_sent],
            (bytes_to_send - total_bytes_sent),
            0)) == -1)
        {
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of message failed.");
                return status::ERROR;
            }
        } else{
            total_bytes_sent += sent_bytes;
        }
    }
    return status::SUCCESS;
}

/*
Prints message from client. Doesn't move reading pointer.
*/
bool Server::printMessageFromClient(int client_socket){
    client_ = client_sockets_.getNode(client_socket);
    uint32_t temp_reading_pointer = client_->reading_pointer_;
    uint8_t temp_reading_buffer = client_->reading_buffer_;
    std::cout << "Message: ";
    for(int i = 0; i < client_->payload_length_; i++){
        std::cout << static_cast<char>(buffer_pool_[client_->reading_pointer_]);
        if(!advanceClientPointer(client_socket)){
            client_->reading_pointer_ = temp_reading_pointer;
            client_->reading_buffer_ = temp_reading_buffer;
            return false;
        }
    }
    std::cout << std::endl;
    client_->reading_pointer_ = temp_reading_pointer;
    client_->reading_buffer_ = temp_reading_buffer;
    return true;
}

bool Server::printClientInformation(int client_socket){
    if(client_socket == -1){
        return false;
    }
    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return false;
    }
    std::cout
    //<< "Name: " << client_->name_ << std::endl
    << "Key: " << static_cast<uint>(client_->sender_key_) << std::endl
    //<< "IP: " << client_->ip_ << std::endl
    //<< "Port: " << client_->port_ << std::endl
    << "Socket: " << client_socket << std::endl
    << std::endl;
    client_ = nullptr;
    return true;
}

unsigned long Server::stringHash(char *str){
    unsigned long hash = 5381;
    int c;
    while (c = *str++) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}