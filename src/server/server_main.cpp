#include <iostream>
#include "../../headers/server.hpp"

int main(){
    Server server;
    if(server.setupServer()){
        server.centralLoop();
    }
    return 0;
}