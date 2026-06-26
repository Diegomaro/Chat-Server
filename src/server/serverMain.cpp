#include <iostream>
#include "../../headers/server.hpp"

int main(){
    Server server;
    int ans = 1;
    int receive_state = 0;

    if(server.setupListenerSocket()){
        server.loopConnections();
        /*while(ans != 0){
            if((receive_state = server.receiveFromClient()) == Constants::ERROR_RETURN){
                std::cout << "Connection failed!" << std::endl;
                return 1;
            } else if(receive_state == Constants::CLOSED_RETURN){
                std::cout << "Client closed connection!" << std::endl;
                break;
            }
            if(!server.sendAcknowledgement()){
                std::cout << "Could not send acknowledgement!" << std::endl;
                return 0;
            }
        }*/
    }
    return 0;
}