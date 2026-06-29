#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include <cerrno>

#include <fcntl.h>

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

    client_addr_length_ = sizeof(client_addr_);

    listener_socket_ = 0;
    pending_client_ = 0;

    epoll_fd_ = 0;

    memset(&server_name_, 0, sizeof(server_name_));
    memset(&client_name_, 0, sizeof(client_name_));

    client_sockaddr_len_ = sizeof(client_sockaddr_);

    bytes_received_ = 0;
    memset(&msg_buffer_, 0, sizeof(msg_buffer_));
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
}

bool Server::setupHashTable(){
    if(!client_sockets_.createTable(16)) {
        return false;
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
                        case Constants::PERROR:{
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
                    rcvf_state_ = receiveFromClient(events_[i].data.fd);
                    switch(rcvf_state_){
                        case Constants::SUCCESS:{
                            if(!printMessageFromClient()){
                                return false;
                            }
                        } break;
                        case Constants::NOTHING_TO_READ:{
                            ack_state_ = sendAcknowledgement(events_[i].data.fd);
                            switch(ack_state_){
                                case Constants::INCOMPLETE_MESSAGE_RESEND:{
                                    // handle later
                                } break;
                                case Constants::PERROR:{
                                    return false;
                                } break;
                                case Constants::INVALID_CLIENT:{
                                    return false;
                                } break;
                            }
                            //return true; // to test for memory leaks
                            receive_loop_ = false;
                        } break;
                        case Constants::INVALID_CLIENT: {
                            return false;
                        }break;
                        case Constants::CLOSED_CONVERSATION: {
                            if(!closeConnection(events_[i].data.fd)){
                                return false;
                            }
                            receive_loop_ = false;
                        } break;
                        case Constants::PERROR:{
                            return false;
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
            return Constants::PERROR;
        }
    }
    if(!addClient()){
        return Constants::EXCEEDED_CLIENT_MAX;
    }

    if(fcntl(pending_client_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return Constants::PERROR;
    }
    ev_.data.fd = pending_client_;
    ev_.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pending_client_, &ev_) == -1){
        perror("epoll failed");
        return Constants::PERROR;
    }
    return Constants::SUCCESS;
}

bool Server::closeConnection(int client_socket){
    if(close(client_socket) == -1){
        perror("clossing failed");
        return false;
    }
    if(!client_sockets_.deleteNode(client_socket)){
        return false;
    }
    std::cout << "Closed connection with client "  << client_socket << std::endl;
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

    if(!client_sockets_.insertNode(pending_client_, new_client)){
        return false;
    }
    return true;
}

// returns INVALID_CLIENT, PERROR, NOTHING_TO_READ, CLOSED_CONVERSATION, SUCCESS
int Server::receiveFromClient(int client_socket){
    if(client_socket == -1){
        return Constants::INVALID_CLIENT;
    }
    bytes_received_ = 0;
    msg_buffer_[0] = '\0';
    if((bytes_received_ = recv(client_socket, msg_buffer_, sizeof(msg_buffer_) - 1, 0)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return Constants::NOTHING_TO_READ;
        } else{
            perror("An error ocurred while receiving from client.");
            return Constants::PERROR;
        }
    }
    if(bytes_received_ == 0){
        return Constants::CLOSED_CONVERSATION;
    }
    return Constants::SUCCESS;
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
            return Constants::PERROR;
        }
    }
    if(bytes_sent != Commands::ACK_LENGTH){
        return Constants::INCOMPLETE_MESSAGE_RESEND;
    }
    return Constants::SUCCESS;
}

bool Server::printClientInformation(int client_socket){
    if(client_socket == -1){
        return false;
    }
    const Client *client = client_sockets_.getNode(client_socket);
    if(!client){
        return false;
    }
    std::cout << "Client Name: " << client->name_ << std::endl;
    std::cout << "Client IP: " << client->ip_ << std::endl;
    std::cout << "Client Port: " << client->port_ << std::endl;
    std::cout << "Client Socket: " << client_socket << std::endl;
    return true;
}

bool Server::printMessageFromClient(){
    if(bytes_received_ <= 0){
        return false;
    }
    if(bytes_received_ > 0){
        msg_buffer_[bytes_received_] = '\0';
        std::cout << "Message received: " << msg_buffer_ << std::endl;
    }
    return true;
}