#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <iostream>

#include <fcntl.h>

#include "../../headers/server.hpp"

Server::Server(){
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    bytes_received = 0;
    current_client = 0;
    memset(&msg_buffer, 0, sizeof(msg_buffer));
    client_addr_len = sizeof(client_addr);
    for(int i = 0; i < Constants::HOST_TOTAL; i++){
        client_sockets[i] = -1;
    }
}

Server::~Server(){
    if(listener_socket != -1){
        close(listener_socket);
    }
    if(epoll_socket != -1){
        close(epoll_socket);

    }
    for(int i = 0; i < Constants::HOST_TOTAL; i++){
        if(client_sockets[i] != -1){
            close(client_sockets[i]);
        }
    }
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
    if ((epoll_socket = epoll_create1(0)) == -1) {
        perror("epoll failed");
        return false;
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listener_socket;
    if(listen(listener_socket, Constants::BACKLOG) == -1){
        perror("listen failed");
        return false;
    }
    epoll_ctl(epoll_socket, EPOLL_CTL_ADD, listener_socket, &ev);
    return true;
}

bool Server::loopConnections(){
    while(true){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_socket, events, 10, -1)) == -1){
            perror("epoll wait failed");
            return false;
        }
        for (int i = 0; i < ready_polls; i++) {
            if(events[i].data.fd == listener_socket){
                if(!acceptConnection()){
                    return false;
                }
                continue;
            }
            int sender_socket = events[i].data.fd;
            if (events[i].events & EPOLLIN) {
                std::cout << "socket " << sender_socket << " is ready to be read!" << std::endl;
                int rcvf_state = 0;
                while(true){
                    rcvf_state = receiveFromClient(events[i].data.fd);
                    if(rcvf_state == Constants::SUCCESS_RETURN){
                        printMessageFromClient();
                    } else if(rcvf_state == Constants::NOTHING_TO_READ_RETURN){
                        sendAcknowledgement(events[i].data.fd);
                        break;
                    } else if(rcvf_state == Constants::CLOSED_RETURN){
                        if(!closeConnection(events[i].data.fd)){
                            return false;
                        }
                        break;
                    }
                }
            }
        }
    }
    return true;
}

bool Server::acceptConnection(){
    if((client_sockets[current_client] = accept(listener_socket, (struct sockaddr *)&client_addr, &client_addr_len)) == -1){
        perror("accept failed");
        return false;
    }
    if(fcntl(client_sockets[current_client], F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return false;
    }
    ev.data.fd = client_sockets[current_client];
    ev.events = EPOLLIN;
    if(epoll_ctl(epoll_socket, EPOLL_CTL_ADD, client_sockets[current_client], &ev) == -1){
        perror("epoll failed");
        return false;
    }
    if(current_client + 1 >= Constants::HOST_TOTAL){
        return false;
    }
    current_client += 1;
    return true;
}

bool Server::closeConnection(int client_socket){
    if(close(client_socket) == -1){
        return false;
    }
    std::cout << "Closed connection with client "  << client_socket << std::endl;
    return true;
}

int Server::receiveFromClient(int client_socket){
    bytes_received = 0;
    msg_buffer[0] = '\0';
    if((bytes_received = recv(client_socket, msg_buffer, sizeof(msg_buffer) - 1, 0)) == -1){
        return Constants:: NOTHING_TO_READ_RETURN;
    }
    if(bytes_received == 0){
        return Constants::CLOSED_RETURN;
    }
    return Constants::SUCCESS_RETURN;
}

bool Server::printMessageFromClient(){
    if(bytes_received > 0){
        msg_buffer[bytes_received] = '\0';
        std::cout << "Message received: " << msg_buffer << std::endl;
    }
    return true;
}

bool Server::sendAcknowledgement(int client_socket){
    int bytes_sent = 0;
    if((bytes_sent = send(client_socket, Commands::ACK, Commands::ACK_LENGTH, 0)) == -1){
        perror("send failed");
        return false;
    }
    return true;
}