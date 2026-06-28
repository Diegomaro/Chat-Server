#include <arpa/inet.h>
#include <stdio.h>

#include "../../headers/client.hpp"

Client::Client(){
    memset(&socket_info, 0, sizeof(socket_info));
    socket_info_length = sizeof(socket_info);
    port = 0;
    memset(&name, 0, sizeof(name));
}