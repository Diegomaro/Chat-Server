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
    accept_state = 0;
    accept_loop = true;
    rcvf_state = 0;
    sender_socket = 0;
    receive_loop = true;
    ack_state = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    client_addr_length = sizeof(client_addr);

    listener_socket = 0;
    pending_client = 0;

    epoll_fd = 0;

    memset(&server_name, 0, sizeof(server_name));
    memset(&client_name, 0, sizeof(client_name));

    client_sockaddr_len = sizeof(client_sockaddr);

    bytes_received = 0;
    memset(&msg_buffer, 0, sizeof(msg_buffer));
}

Server::~Server(){
    if(listener_socket != -1){
        close(listener_socket);
    }
    if(epoll_fd != -1){
        close(epoll_fd);
    }

    client_sockets.resetNodeIndex();
    while(client_sockets.hasNodes()){
        if(client_sockets.hasNode()){
            int socket = (client_sockets.getNode()->key);
            if(socket != -1){
                std::cout << "closing socket: " << socket << std::endl;
                close(socket);
            }
        }
        client_sockets.advanceNode();
    }
    std::cout << "stopped" << std::endl;
}

bool Server::setupHashTable(){
    if(!client_sockets.createTable(16)) {
        return false;
    }
    return true;
}

bool Server::setupListenerSocket(){
    int status = 0;
    if((status = getaddrinfo(NULL, Constants::SERVER_PORT, &hints, &res)) != 0){
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        return false;
    }
    if((listener_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1){
        perror("socket failed");
        return false;
    }
    int yes = 1;
    if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt SO_REUSEADDR failed");
    }
    if((bind(listener_socket, res->ai_addr, res->ai_addrlen)) == -1){
        perror("bind failed");
        return false;
    }
    if(fcntl(listener_socket, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return false;
    }
    freeaddrinfo(res);
    if ((epoll_fd = epoll_create1(0)) == -1) {
        perror("epoll failed");
        return false;
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listener_socket;
    if(listen(listener_socket, Constants::BACKLOG) == -1){
        perror("listen failed");
        return false;
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener_socket, &ev);
    return true;
}

bool Server::loopConnections(){
    while(true){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_fd, events, Constants::MAX_EVENTS, -1)) == -1){
            perror("epoll wait failed");
            return false;
        }
        for (int i = 0; i < ready_polls; i++) {
            if(events[i].data.fd == listener_socket){
                accept_state = 0;
                accept_loop = true;
                while(accept_loop){
                    accept_state = acceptConnection();
                    switch(accept_state){
                        case Constants::SUCCESS:{
                            if(!printClientInformation(pending_client)){
                                return false;
                            }
                        } break;
                        case Constants::NOTHING_TO_READ:{
                            accept_loop = false;
                        } break;
                        case Constants::PERROR:{
                            return false;
                        } break;
                        case Constants::EXCEEDED_CLIENT_MAX:{
                            return false;
                        } break;
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                sender_socket = events[i].data.fd;
                rcvf_state = 0;
                receive_loop = true;
                while(receive_loop){
                    rcvf_state = receiveFromClient(events[i].data.fd);
                    switch(rcvf_state){
                        case Constants::SUCCESS:{
                            if(!printMessageFromClient()){
                                return false;
                            }
                        } break;
                        case Constants::NOTHING_TO_READ:{
                            ack_state = sendAcknowledgement(events[i].data.fd);
                            switch(ack_state){
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
                            receive_loop = false;
                        } break;
                        case Constants::INVALID_CLIENT: {
                            return false;
                        }break;
                        case Constants::CLOSED_CONVERSATION: {
                            if(!closeConnection(events[i].data.fd)){
                                return false;
                            }
                            receive_loop = false;
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
    if(client_sockets.getDataCount() + 1 >= Constants::MAX_HOSTS){
        return Constants::EXCEEDED_CLIENT_MAX;
    }
    pending_client = 0;
    if((pending_client = accept(listener_socket, (struct sockaddr *)&client_sockaddr, &client_sockaddr_len)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return Constants::NOTHING_TO_READ;
        } else{
            perror("accept failed");
            return Constants::PERROR;
        }
    }
    if(!addClient()){
        return Constants::EXCEEDED_CLIENT_MAX; // modify error
    }

    if(fcntl(pending_client, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return Constants::PERROR;
    }
    ev.data.fd = pending_client;
    ev.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pending_client, &ev) == -1){
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
    std::cout << "Closed connection with client "  << client_socket << std::endl;
    return true;
}

bool Server::addClient(){
    Client new_client;
    std::strcpy(new_client.name, Constants::NOT_NAMED);
    void* addr;

    if(client_sockaddr.ss_family == AF_INET) {
        sockaddr_in* ipv4 = (sockaddr_in*)&client_sockaddr;
        addr = &(ipv4->sin_addr);
        new_client.port = ntohs(ipv4->sin_port);
    }
    else {
        sockaddr_in6* ipv6 = (sockaddr_in6*)&client_sockaddr;
        addr = &(ipv6->sin6_addr);
        new_client.port = ntohs(ipv6->sin6_port);
    }
    inet_ntop(client_sockaddr.ss_family, addr, new_client.ip, sizeof(new_client.ip));

    if(!client_sockets.insertNode(pending_client, new_client)){
        return false;
    }
    return true;
}

// returns INVALID_CLIENT, PERROR, NOTHING_TO_READ, CLOSED_CONVERSATION, SUCCESS
int Server::receiveFromClient(int client_socket){
    if(client_socket == -1){
        return Constants::INVALID_CLIENT;
    }
    bytes_received = 0;
    msg_buffer[0] = '\0';
    if((bytes_received = recv(client_socket, msg_buffer, sizeof(msg_buffer) - 1, 0)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return Constants::NOTHING_TO_READ;
        } else{
            perror("An error ocurred while receiving from client.");
            return Constants::PERROR;
        }
    }
    if(bytes_received == 0){
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
    const Client *client = client_sockets.getNode(client_socket);
    if(!client){
        return false;
    }
    std::cout << "Client Name: " << client->name << std::endl;
    std::cout << "Client IP: " << client->ip << std::endl;
    std::cout << "Client Port: " << client->port << std::endl;
    std::cout << "Client Socket: " << client_socket << std::endl;
    return true;
}

bool Server::printMessageFromClient(){
    if(bytes_received <= 0){
        return false;
    }
    if(bytes_received > 0){
        msg_buffer[bytes_received] = '\0';
        std::cout << "Message received: " << msg_buffer << std::endl;
    }
    return true;
}