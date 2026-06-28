#include <arpa/inet.h>
#include <stdio.h>

#include "../../headers/client.hpp"

Client::Client(){
    port = 0;
    memset(&name, 0, sizeof(name));
    memset(&ip, 0, sizeof(ip));
}