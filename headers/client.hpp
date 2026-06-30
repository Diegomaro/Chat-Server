#pragma once
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include "constants.hpp"

struct Client{
    public:
        Client();
        char name_ [Constants::MAX_HOSTNAME_LENGTH];
        char ip_ [INET6_ADDRSTRLEN];
        int port_;
        char *buffer_pointers_[10];
};