#pragma once
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include "constants.hpp"

struct Client{
    public:
        Client();
        struct sockaddr_storage socket_info;
        socklen_t  socket_info_length;
        char name [Constants::MAX_HOSTNAME_LENGTH];
        char ip [INET6_ADDRSTRLEN];
        int port;
};