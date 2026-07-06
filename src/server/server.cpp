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
    return true;
}

bool Server::setupBuffer(){
    if(buffer_pool_ || msg_buffer_){
        return false;
    }
    buffer_pool_ = new(std::nothrow) uint8_t [cts::BUFFER_SIZE];
    if(!buffer_pool_){
        return false;
    }
    msg_buffer_ = new(std::nothrow) uint8_t [cts::BUFFER_READING_SIZE];
    if(!msg_buffer_){
        return false;
    }

    uint32_t current_address = 0;
    for(int i = 0; i < cts::AMOUNT_OF_BUFFERS; i++){
        if(!available_buffers_.insertTail(current_address)){
            return false;
        }
        current_address += 0x00000200;
    }
    return true;
}

bool Server::setupListenerSocket(){
    int status = 0;
    if((status = getaddrinfo(NULL, cts::SERVER_PORT, &hints_, &res_)) != 0){
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
    if(listen(listener_socket_, cts::BACKLOG) == -1){
        perror("listen failed");
        return false;
    }
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listener_socket_, &ev_);
    return true;
}

bool Server::loopConnections(){
    while(true){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_fd_, events_, cts::MAX_EVENTS, -1)) == -1){
            perror("epoll wait failed");
            return false;
        }
        for (int i = 0; i < ready_polls; i++) {
            if(events_[i].data.fd == listener_socket_){
                bool accept_loop = true;
                while(accept_loop){
                    uint8_t accept_state = acceptConnection();
                    switch(accept_state){
                        case rts::SUCCESS:{
                            if(!printClientInformation(pending_client_)){
                                return false;
                            }
                        } break;
                        case rts::NOTHING_TO_READ:{
                            accept_loop = false;
                        } break;
                        case rts::ERROR:{
                            return false;
                        } break;
                        case rts::EXCEEDED_CLIENT_MAX:{
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
                        case rts::SUCCESS:{
                            if(checkMessage(sender_socket) == rts::SUCCESS){
                                // cannot send messages until authenticated
                                if(!printMessageFromClient(sender_socket)){
                                    return false;
                                }
                                int send_state = sendToClient(sender_socket);
                                switch(send_state){
                                    case rts::RESOURCE_UNAVAILABLE:{
                                        //handle alter, should be stored instead of sending.
                                        return false;
                                    } break;
                                    case rts::ERROR:{
                                        return false;
                                    } break;
                                }
                                if(!cleanClientBuffer(sender_socket)){
                                    return false;
                                }
                            }
                           //if missing timeout
                        } break;
                        case rts::NOTHING_TO_READ:{
                            /*
                            uint8_t ack_state = sendAcknowledgement(sender_socket);
                            switch(ack_state){
                                case rts::INCOMPLETE_MESSAGE:{
                                    // handle later
                                } break;
                                case rts::ERROR:{
                                    return false;
                                } break;
                                case rts::INVALID_CLIENT:{
                                    return false;
                                } break;
                            }
                            */
                            receive_loop = false;
                        } break;
                        case rts::INVALID_CLIENT:{
                            return false;
                        }break;
                        case rts::CLOSED_CONVERSATION:{
                            if(!closeConnection(sender_socket)){
                                return false;
                            }
                            receive_loop = false;
                            return true; // to test for memory leaks
                        } break;
                        case rts::ERROR:{
                            return false;
                        } break;
                        case rts::EXCEEDED_CLIENT_BUFFER_SIZE:{
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
    if(client_sockets_.getDataCount() + 1 >= cts::MAX_HOSTS){
        return rts::EXCEEDED_CLIENT_MAX;
    }
    pending_client_ = 0;
    if((pending_client_ = accept(listener_socket_, (struct sockaddr *)&client_sockaddr_, &client_sockaddr_len_)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return rts::NOTHING_TO_READ;
        } else{
            perror("accept failed");
            return rts::ERROR;
        }
    }
    if(!addClient()){
        return rts::EXCEEDED_CLIENT_MAX;
    }

    if(fcntl(pending_client_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return rts::ERROR;
    }
    ev_.data.fd = pending_client_;
    ev_.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pending_client_, &ev_) == -1){
        perror("epoll failed");
        return rts::ERROR;
    }
    return rts::SUCCESS;
}

bool Server::closeConnection(int client_socket){
    std::cout << "attempting to close: " << client_socket << std::endl;
    if(close(client_socket) == -1){
        perror("clossing failed");
        return false;
    }
    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return false;
    }
    int8_t buffers_erased = 0;
    for(int i = 0; i < cts::CLIENT_POINTERS; i++){
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
    if(!client_sockets_.deleteNode(client_socket)){
        return false;
    }
    std::cout << "manual close of socket 7 "  << client_socket << std::endl;
    client_ = nullptr;
    return true;
}

bool Server::addClient(){
    Client new_client;
    std::strcpy(new_client.name_, cts::NOT_NAMED);
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
    // Temporal solution until keys given are stored in non volatile memory
    new_client.sender_key_ = current_client_id_;
    current_client_id_++;

    if(!client_sockets_.insertNode(pending_client_, new_client)){
        return false;
    }
    if(!client_key_to_client_sockets_.insertNode(new_client.sender_key_, pending_client_)){
        std::cout << "failed here" << std::endl;
        return false;
    }
    return true;
}

// returns INVALID_CLIENT, PERROR, NOTHING_TO_READ, CLOSED_CONVERSATION, SUCCESS, EXCEEDED_CLIENT_BUFFER_SIZE, INSUFFICIENT_BUFFER_SPACE
int Server::receiveFromClient(int client_socket){
    if(client_socket == -1){
        return rts::INVALID_CLIENT;
    }

    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return rts::ERROR;
    }
    msg_buffer_[0] = '\0';
    if((bytes_received_ = recv(client_socket, msg_buffer_, cts::BUFFER_READING_SIZE, 0)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return rts::NOTHING_TO_READ;
        } else{
            perror("An error ocurred while receiving from client.");
            return rts::ERROR;
        }
    }
    std::cout << "bytes received: " << bytes_received_ << std::endl;
    if(bytes_received_ == 0){
        return rts::CLOSED_CONVERSATION;
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
            if(client_->buffer_pointers_amount_ + 1 >= cts::CLIENT_POINTERS){
                if(checkMessage(client_socket) == rts::SUCCESS){
                    std::cout << "work on it later" << std::endl;
                    // delete old message
                }
                return rts::EXCEEDED_CLIENT_BUFFER_SIZE;
            } else if(available_buffers_.isEmpty()){
                return rts::INSUFFICIENT_BUFFER_SPACE;
            } else{
                uint32_t new_buffer_segment = available_buffers_.getHead();
                client_->buffer_pointers_[(client_->writing_buffer_ + 1) % 128] = new_buffer_segment;
                if(!available_buffers_.deleteHead()){
                    return rts::ERROR;
                }
                client_->buffer_pointers_amount_++;
                client_->writing_buffer_ = (client_->writing_buffer_ + 1) % 128;
                client_->writing_pointer_ = new_buffer_segment;
            }
        }
    }
    client_ = nullptr;
    return rts::SUCCESS;
}

int Server::checkMessage(int client_socket){
    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return rts::ERROR;
    }
    if(client_->byte_counter_ < 8){
        return rts::INVALID_MESSAGE;
    }
    // HEAD_BITS
    if((buffer_pool_[client_->reading_pointer_] ^ 0xFF) != 0){
        return rts::INVALID_MESSAGE;
    }
    if(!advanceClientPointer(client_socket)){
        return rts::INVALID_MESSAGE; // other error
    }

    // TYPE
    if(client_->type_ == 0){
        client_->type_ = buffer_pool_[client_->reading_pointer_];
    }
    if(!advanceClientPointer(client_socket)){
        return rts::INVALID_MESSAGE;
    }

    // HOST_KEY
    if(client_->receiver_key_ == UINT32_MAX){
        uint32_t tmp_pointer = client_->reading_pointer_;
        uint8_t tmp_buffer = client_->reading_buffer_;

        client_->receiver_key_ = 0;

        for(int i = 0; i < cts::CLIENT_KEY_LENGTH; i++){
            client_->receiver_key_ += (buffer_pool_[client_->reading_pointer_]) << ((cts::CLIENT_KEY_LENGTH - 1 - i) * 8);
            if(!advanceClientPointer(client_socket)){
                return rts::INVALID_MESSAGE;
            }
        }
        // repopulate the hash table each time the server opens
        if(!client_key_to_client_sockets_.searchNode(client_->receiver_key_)){
            return rts::INVALID_CLIENT;
        }
        client_->receiver_fd_ = *client_key_to_client_sockets_.getNode(client_->receiver_key_);

        client_->reading_pointer_ = tmp_pointer;
        client_->reading_buffer_ = tmp_buffer;

        for(int i = 0; i < cts::CLIENT_KEY_LENGTH; i++){
            buffer_pool_[client_->reading_pointer_] = client_->sender_key_ << ((cts::CLIENT_KEY_LENGTH - 1 - i) * 8);
            if(!advanceClientPointer(client_socket)){
                return rts::INVALID_MESSAGE;
            }
        }


    } else{
        for(int i = 0; i < 4; i++){
            if(!advanceClientPointer(client_socket)){
                return rts::INVALID_MESSAGE;
            }
        }
    }

    // PAYLOAD_LENGTH
    if(client_->payload_length_ == UINT16_MAX){
        client_->payload_length_ = 0;
        client_->payload_length_ = buffer_pool_[client_->reading_pointer_] << 8;
        if(!advanceClientPointer(client_socket)){
            return rts::INVALID_MESSAGE;
        }
        client_->payload_length_ = client_->payload_length_ | (buffer_pool_[client_->reading_pointer_]);
    } else{
        if(!advanceClientPointer(client_socket)){
            return rts::INVALID_MESSAGE;
        }
    }
    if(!advanceClientPointer(client_socket)){
        return rts::INVALID_MESSAGE;
    }

    switch(client_->type_){
        case tys::USER:{
            if(client_->payload_length_ == 0){
                return rts::INVALID_MESSAGE;
            }

            if(!client_sockets_.searchNode(client_->receiver_fd_)){
                return rts::INVALID_CLIENT;
                // later it should be changed to store all client keys.
                // If client is not available it should be stored in some file. (much later)
            }
            if(client_->byte_counter_ < client_->payload_length_ + cts::HEADER_SIZE){
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
    return rts::ERROR;
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
    client_->byte_counter_ -= client_->payload_length_ + cts::HEADER_SIZE;

    client_->resetMessage();
    return true;
}

bool Server::advanceClientPointer(int client_socket){
    if(!client_->advanceReadingPointer()){
        client_->reading_buffer_++;
        if(client_->reading_buffer_ >= cts::CLIENT_POINTERS){
            return false;
        } else{
            client_->reading_pointer_ = client_->buffer_pointers_[client_->reading_buffer_];
        }
    }
    return true;
}

// returns
int Server::sendAcknowledgement(int client_socket){
    /*if(client_socket == -1){
        return rts::INVALID_CLIENT;
    }
    int bytes_sent = 0;
    if((bytes_sent = send(client_socket, Commands::ACK, Commands::ACK_LENGTH, 0)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return cts::NOTHING_TO_READ;
        } else{
            perror("Send of acknowledgement failed.");
            return cts::ERROR;
        }
    }
    if(bytes_sent != Commands::ACK_LENGTH){
        return cts::INCOMPLETE_MESSAGE;
    }*/
    return rts::SUCCESS;
}

/*
Sends message from one client to another.
Return values: SUCCESS, ERROR, RESOURCE_UNAVAILABLE
*/
int Server::sendToClient(int client_socket){
    client_ = client_sockets_.getNode(client_socket);

    int print_pointer = cts::READER_BUFFER_POINTER;
    int bytes_to_send = client_->payload_length_ + cts::HEADER_SIZE;
    int bytes_sent = 0;

    client_->reading_pointer_ = client_->starting_pointer_;
    for(int i = 0; i <  bytes_to_send; i++){
        buffer_pool_[print_pointer] = buffer_pool_[client_->reading_pointer_];
        if(!advanceClientPointer(client_socket)){
            return rts::ERROR;
        }
        print_pointer++;
    }

    std::cout << "destination socket: " << client_->receiver_fd_ << std::endl;

    while(bytes_sent < bytes_to_send){
        int sent_bytes = 0;
        if((sent_bytes = send(
            client_->receiver_fd_,
            &buffer_pool_[cts::READER_BUFFER_POINTER + bytes_sent],
            (bytes_to_send - bytes_sent),
            0)) == -1)
        {
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return rts::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of message failed.");
                return rts::ERROR;
            }
        } else{
            bytes_sent += sent_bytes;
        }
    }
    std::cout << "Message sent." << std::endl;
    return rts::SUCCESS;
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
    std::cout << std::endl << std::endl;
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
    std::cout << "Name: " << client_->name_ << std::endl
    << "Key: " << static_cast<uint>(client_->sender_key_) << std::endl
    << "IP: " << client_->ip_ << std::endl
    << "Port: " << client_->port_ << std::endl
    << "Socket: " << client_socket << std::endl;
    client_ = nullptr;
    return true;
}