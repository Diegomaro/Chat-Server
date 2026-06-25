#include "../../headers/server.hpp"

int main(){
    Server server;
    if(server.setupListenerSocket()){
        if(server.awaitConnection()){
            server.receiveFromPeer();
            server.sendAcknowledgement();
            server.closeConnection();
        }
    }
    return 0;
}