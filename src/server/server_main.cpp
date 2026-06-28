#include <iostream>
#include "../../headers/server.hpp"

int main(){
    Server server;
    if(server.setupListenerSocket()){
        server.loopConnections();
    }
    return 0;
}