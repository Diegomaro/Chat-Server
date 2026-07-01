#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include <cerrno>

#include <fcntl.h>

#include <iomanip>
#include <cstdint>

#include "../../headers/server.hpp"

Server::Server(){
    accept_state_ = 0;
    accept_loop_ = true;
    rcvf_state_ = 0;
    sender_socket_ = 0;
    receive_loop_ = true;
    ack_state_ = 0;

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
                std::cout << "closing socket: " << socket << std::endl;
                close(socket);
            }
        }
        client_sockets_.advanceNode();
    }
    if(buffer_pool_){
        delete [] buffer_pool_;
    }
    if(msg_buffer_){
        delete [] msg_buffer_;
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
    buffer_pool_ = new(std::nothrow) uint8_t [Constants::BUFFER_SIZE];
    if(!buffer_pool_){
        return false;
    }
    msg_buffer_ = new(std::nothrow) uint8_t [Constants::BUFFER_SEGMENT_SIZE];
    if(!msg_buffer_){
        return false;
    }

    uint32_t current_address = 0x00000000;
    for(int i = 0; i < Constants::STARTING_BUFFERS; i++){
        if(!available_buffers_.insertTail(current_address)){
            return false;
        }
        current_address += 0x00000200;
    }
    return true;
}

bool Server::setupListenerSocket(){
    int status = 0;
    if((status = getaddrinfo(NULL, Constants::SERVER_PORT, &hints_, &res_)) != 0){
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
    if(listen(listener_socket_, Constants::BACKLOG) == -1){
        perror("listen failed");
        return false;
    }
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listener_socket_, &ev_);
    return true;
}

bool Server::loopConnections(){
    while(true){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_fd_, events_, Constants::MAX_EVENTS, -1)) == -1){
            perror("epoll wait failed");
            return false;
        }
        for (int i = 0; i < ready_polls; i++) {
            if(events_[i].data.fd == listener_socket_){
                accept_state_ = 0;
                accept_loop_ = true;
                while(accept_loop_){
                    accept_state_ = acceptConnection();
                    switch(accept_state_){
                        case Constants::SUCCESS:{
                            if(!printClientInformation(pending_client_)){
                                return false;
                            }
                        } break;
                        case Constants::NOTHING_TO_READ:{
                            accept_loop_ = false;
                        } break;
                        case Constants::ERROR:{
                            return false;
                        } break;
                        case Constants::EXCEEDED_CLIENT_MAX:{
                            return false;
                        } break;
                    }
                }
            } else if (events_[i].events & EPOLLIN) {
                sender_socket_ = events_[i].data.fd;
                rcvf_state_ = 0;
                receive_loop_ = true;
                while(receive_loop_){
                    rcvf_state_ = receiveFromClient(sender_socket_);
                    switch(rcvf_state_){
                        case Constants::SUCCESS:{
                            if(checkMessage(sender_socket_) == Constants::SUCCESS){
                                if(!printMessageFromClient(sender_socket_)){ // has to happen
                                    return false;
                                }
                                if(!cleanClientBuffer(sender_socket_)){
                                    return false;
                                }
                            }
                           //if missing timeout
                        } break;
                        case Constants::NOTHING_TO_READ:{
                            ack_state_ = sendAcknowledgement(sender_socket_);
                            switch(ack_state_){
                                case Constants::INCOMPLETE_MESSAGE:{
                                    // handle later
                                } break;
                                case Constants::ERROR:{
                                    return false;
                                } break;
                                case Constants::INVALID_CLIENT:{
                                    return false;
                                } break;
                            }
                            // return true; // to test for memory leaks
                            receive_loop_ = false;
                        } break;
                        case Constants::INVALID_CLIENT: {
                            return false;
                        }break;
                        case Constants::CLOSED_CONVERSATION: {
                            if(!closeConnection(sender_socket_)){
                                return false;
                            }
                            return true; // to test for memory leaks
                            receive_loop_ = false;
                        } break;
                        case Constants::ERROR:{
                            return false;
                        } break;
                        case Constants::EXCEEDED_CLIENT_BUFFER_SIZE:{
                            // return error message to client and restart buffer segments
                        } break;
                    }
                }
            }
        }
    }
    return true;
}

// returns EXCEEDED_CLIENT_MAX, NOTHING_TO_READ, PERROR, SUCCESS
int Server::acceptConnection(){
    if(client_sockets_.getDataCount() + 1 >= Constants::MAX_HOSTS){
        return Constants::EXCEEDED_CLIENT_MAX;
    }
    pending_client_ = 0;
    if((pending_client_ = accept(listener_socket_, (struct sockaddr *)&client_sockaddr_, &client_sockaddr_len_)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return Constants::NOTHING_TO_READ;
        } else{
            perror("accept failed");
            return Constants::ERROR;
        }
    }
    if(!addClient()){
        return Constants::EXCEEDED_CLIENT_MAX;
    }

    if(fcntl(pending_client_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return Constants::ERROR;
    }
    ev_.data.fd = pending_client_;
    ev_.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pending_client_, &ev_) == -1){
        perror("epoll failed");
        return Constants::ERROR;
    }
    return Constants::SUCCESS;
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
    for(int i = 0; i < Constants::CLIENT_POINTERS; i++){
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
    if(!client_sockets_.deleteNode(client_socket)){
        return false;
    }
    if(!client_key_to_client_sockets_.deleteNode(client_->sender_key_)){
        return false;
    }
    std::cout << "Closed connection with client "  << client_socket << std::endl;
    client_ = nullptr;
    return true;
}

bool Server::addClient(){
    Client new_client;
    std::strcpy(new_client.name_, Constants::NOT_NAMED);
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
    new_client.writing_pointer_ = new_client.buffer_pointers_[0];
    new_client.sender_key_ = new_client.buffer_pointers_[0];
    if(!client_sockets_.insertNode(pending_client_, new_client)){
        return false;
    }
    //if(!client_key_to_client_sockets_.insertNode(new_client.sender_key_, pending_client_)){
    if(!client_key_to_client_sockets_.insertNode(0, pending_client_)){
        return false;
    }
    return true;
}

// returns INVALID_CLIENT, PERROR, NOTHING_TO_READ, CLOSED_CONVERSATION, SUCCESS, EXCEEDED_CLIENT_BUFFER_SIZE, INSUFFICIENT_BUFFER_SPACE
int Server::receiveFromClient(int client_socket){
    if(client_socket == -1){
        return Constants::INVALID_CLIENT;
    }
    bytes_received_ = 0;

    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return false;
    }
    msg_buffer_[0] = '\0';
    if((bytes_received_ = recv(client_socket, msg_buffer_, Constants::BUFFER_SEGMENT_SIZE, 0)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return Constants::NOTHING_TO_READ;
        } else{
            perror("An error ocurred while receiving from client.");
            return Constants::ERROR;
        }
    }
    if(bytes_received_ == 0){
        return Constants::CLOSED_CONVERSATION;
    }
    for(int i = 0; i < bytes_received_; i++){
        buffer_pool_[client_->writing_pointer_] = msg_buffer_[i];
        client_->byte_counter_++;
        if(!client_->advanceWritingPointer()){
            if(client_->buffer_pointers_amount_ + 1 >= Constants::CLIENT_POINTERS){
                return Constants::EXCEEDED_CLIENT_BUFFER_SIZE;
            } else{
                if(available_buffers_.isEmpty()){
                    return Constants::INSUFFICIENT_BUFFER_SPACE;
                }
                uint32_t new_buffer_segment = available_buffers_.getHead();
                client_->buffer_pointers_[client_->writing_buffer_ + 1] = new_buffer_segment;
                if(!available_buffers_.deleteHead()){
                    return Constants::ERROR;
                }
                client_->writing_buffer_++;
                client_->writing_pointer_ = new_buffer_segment;
            }
        }
    }
    client_ = nullptr;
    return Constants::SUCCESS;
}

int Server::checkMessage(int client_socket){
    client_ = client_sockets_.getNode(client_socket); // remove all of these
    if(!client_){
        return Constants::ERROR;
    }

    // HEAD_BITS
    if((buffer_pool_[client_->reading_pointer_] ^ 0xFF) != 0){
        return Constants::INVALID_MESSAGE;
    }
    if(!advanceClientPointer(client_socket)){
        return Constants::INVALID_MESSAGE;
    }

    // TYPE
    if(client_->type_ == 0){
        client_->type_ = buffer_pool_[client_->reading_pointer_];
    }
    if(!advanceClientPointer(client_socket)){
        return Constants::INVALID_MESSAGE;
    }

    // HOST_KEY
    if(client_->receiver_key_ == UINT32_MAX){
        client_->receiver_key_ = 0;
        for(int i = 0; i < 4; i++){
            client_->receiver_key_ = client_->receiver_key_ | (buffer_pool_[client_->reading_pointer_]) << (i * 8);
            if(!advanceClientPointer(client_socket)){
                return Constants::INVALID_MESSAGE;
            }
        }
        if(!client_key_to_client_sockets_.searchNode(client_->receiver_key_)){
            return Constants::INVALID_CLIENT;
        }
        client_->receiver_fd_ = *client_key_to_client_sockets_.getNode(client_->receiver_key_);
    } else{
        for(int i = 0; i < 4; i++){
            if(!advanceClientPointer(client_socket)){
                return Constants::INVALID_MESSAGE;
            }
        }
    }

    // PAYLOAD_LENGTH
    if(client_->payload_length_ == UINT16_MAX){
        client_->payload_length_ = 0;
        client_->payload_length_ = buffer_pool_[client_->reading_pointer_] << 8;
        if(!advanceClientPointer(client_socket)){
            return Constants::INVALID_MESSAGE;
        }
        client_->payload_length_ = client_->payload_length_ | (buffer_pool_[client_->reading_pointer_]);
    } else{
        if(!advanceClientPointer(client_socket)){
            return Constants::INVALID_MESSAGE;
        }
    }
    if(!advanceClientPointer(client_socket)){
        return Constants::INVALID_MESSAGE;
    }

    switch(client_->type_){
        case Types::USER:{
            if(client_->payload_length_ == 0){
                return Constants::INVALID_MESSAGE;
            }
            if(!client_sockets_.searchNode(client_->receiver_fd_)){
                return Constants::INVALID_CLIENT;
                // later it should be changed to store all client keys.
                // If client is not available it should be stored in some file. (much later)
            }
            if(client_->byte_counter_ < client_->payload_length_){
                return Constants::INCOMPLETE_MESSAGE;
            }
            return Constants::SUCCESS;
        } break;
        case Types::GROUP:{
            //implement much later
        } break;
        case Types::AUTH_KEY:{
            // implement much later
        } break;
        case Types::SEND_REQUEST:{
            // implement much later
        } break;
        case Types::ACCEPT_REQUEST:{
            // implement much later
        } break;
        default:{
            return Constants::INVALID_MESSAGE;
        }
    }
    return Constants::SUCCESS;
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
    }
    if(dif > 1){
        client_->buffer_pointers_amount_--;
        client_->buffer_pointers_[client_->starting_buffer_] = UINT32_MAX;
        client_->starting_buffer_ = client_->writing_buffer_;
        client_->reading_buffer_ = client_->starting_buffer_;
    }

    client_->starting_pointer_ = client_->reading_pointer_;
    client_->byte_counter_ -= client_->payload_length_;

    client_->resetMessage();
    return true;

}

bool Server::advanceClientPointer(int client_socket){
    if(client_->reading_pointer_ + 1 >= (client_->buffer_pointers_[client_->reading_buffer_] + Constants::MAX_MESSAGE_SIZE)){
        client_->reading_buffer_++;
        if(client_->reading_buffer_ >= Constants::CLIENT_POINTERS){
            return false;
        } else{
            client_->reading_pointer_ = client_->buffer_pointers_[client_->reading_buffer_];
        }
    } else{
        client_->reading_pointer_++;
    }
    return true;
}


// returns INVALID_CLIENT, PERROR, INCOMPLETE_MESSAGE_RESEND, SUCCESS
int Server::sendAcknowledgement(int client_socket){
    if(client_socket == -1){
        return Constants::INVALID_CLIENT;
    }
    int bytes_sent = 0;
    if((bytes_sent = send(client_socket, Commands::ACK, Commands::ACK_LENGTH, 0)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return Constants::NOTHING_TO_READ;
        } else{
            perror("Send of acknowledgement failed.");
            return Constants::ERROR;
        }
    }
    if(bytes_sent != Commands::ACK_LENGTH){
        return Constants::INCOMPLETE_MESSAGE;
    }
    return Constants::SUCCESS;
}

bool Server::printClientInformation(int client_socket){
    if(client_socket == -1){
        return false;
    }
    client_ = client_sockets_.getNode(client_socket);
    if(!client_){
        return false;
    }
    std::cout << "Client Name: " << client_->name_ << std::endl;
    std::cout << "Client Name: " << client_->sender_key_ << std::endl;
    std::cout << "Client IP: " << client_->ip_ << std::endl;
    std::cout << "Client Port: " << client_->port_ << std::endl;
    std::cout << "Client Socket: " << client_socket << std::endl;
    client_ = nullptr;
    return true;
}

bool Server::printMessageFromClient(int client_socket){
    client_ = client_sockets_.getNode(client_socket);
    int print_pointer = Constants::READER_BUFFER_POINTER;
    for(int i = 0; i < client_->payload_length_; i++){
        buffer_pool_[print_pointer] = buffer_pool_[client_->reading_pointer_];
        if(!advanceClientPointer(client_socket)){
            return false;
        }
        print_pointer++;
    }
    msg_buffer_[bytes_received_] = '\0';
    std::cout << "Message received: " << std::endl;
    for(int i = 0; i < client_->payload_length_; i++){
        std::cout << static_cast<char>(buffer_pool_[Constants::READER_BUFFER_POINTER + i]);
    }
    std::cout << std::endl;
    return true;
}