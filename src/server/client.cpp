#include <arpa/inet.h>
#include <stdio.h>

#include "../../headers/client.hpp"

Client::Client(){
    port_ = 0;
    memset(&name_, 0, sizeof(name_));
    memset(&ip_, 0, sizeof(ip_));
}