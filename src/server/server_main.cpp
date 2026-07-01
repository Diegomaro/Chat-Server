#include <iostream>
#include "../../headers/server.hpp"

int main(){
    Server server;
    if(!server.setupHashTables()){
        return 1;
    }
    if(!server.setupBuffer()){
        return 1;
    }
    if(server.setupListenerSocket()){
        server.loopConnections();
    }
    return 0;
}