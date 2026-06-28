#include <iostream>
#include "../../headers/server.hpp"

int main(){
    Server server;
    server.setupHashTable();
    if(server.setupListenerSocket()){
        server.loopConnections();
    }
    return 0;
}