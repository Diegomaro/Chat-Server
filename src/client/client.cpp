#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#include <iostream>

#include "../../headers/client.hpp"

Client::Client(){
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    memset(&msg_buffer, 0, sizeof(msg_buffer));
    server_addr_len = sizeof(server_addr);
    msg_len = strlen(msg);
}

Client::~Client(){
    close(client_socket);
}

bool Client::setupSocket(){
    int status;
    if((status = getaddrinfo("127.0.0.1", Constants::SERVER_PORT, &hints, &server_info)) != 0){
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        return false;
    }
    if((client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol)) == -1){
        perror("socket failed");
        return false;
    }
    if((connect(client_socket, server_info->ai_addr, server_info->ai_addrlen)) == -1){
        perror("connect failed");
        return false;
    }
    freeaddrinfo(server_info);
    return true;
}

bool Client::sendMessage(){
    int bytes_sent = 0;
    if((bytes_sent = send(client_socket, msg, msg_len, 0)) == -1){
        perror("send failed");
        return false;
    }
    return true;
}

bool Client::receiveFromServer(){
    int bytes_received = 0;
    if((bytes_received = recv(client_socket, msg_buffer, sizeof(msg_buffer) - 1, 0)) == -1){
        perror("received failed");
        return false;
    }
    if(bytes_received > 0){
        msg_buffer[bytes_received] = '\0';
        std::cout << "Message received: " << msg_buffer << std::endl;
    }
    return true;
}