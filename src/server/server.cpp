#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <iostream>


#include "../../headers/server.hpp"

Server::Server(){
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    memset(&msg_buffer, 0, sizeof(msg_buffer));
    peer_addr_len = sizeof(peer_addr);
    ack_msg_len = strlen(ack_msg);
    peer_addr_len = sizeof(peer_addr);
    for(int i = 0; i < Constants::HOST_TOTAL; i++){
        peer_sockets[i] = -1;
    }
}

Server::~Server(){
    closeConnection();
    close(listener_socket);
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
    freeaddrinfo(res);
    return true;
}

bool Server::awaitConnection(){
    if(listen(listener_socket, Constants::BACKLOG) == -1){
        perror("listen failed");
        return false;
    }

    if((peer_sockets[0] = accept(listener_socket, (struct sockaddr *)&peer_addr, &peer_addr_len)) == -1){
        perror("accept failed");
        return false;
    }
    return true;
}

bool Server::closeConnection(){
    if(close(peer_sockets[0]) == -1){
        return false;
    }
    return true;
}

bool Server::receiveFromPeer(){
    int bytes_received = 0;
    if((bytes_received = recv(peer_sockets[0], msg_buffer, sizeof(msg_buffer) - 1, 0)) == -1){
        perror("received failed");
        return false;
    }

    if(bytes_received > 0){
        msg_buffer[bytes_received] = '\0';
        std::cout << "Message received: " << msg_buffer << std::endl;
    }
    return true;
}

bool Server::sendAcknowledgement(){
    int bytes_sent = 0;
    if((bytes_sent = send(peer_sockets[0], ack_msg, ack_msg_len, 0)) == -1){
        perror("send failed");
        return false;
    }
    return true;
}