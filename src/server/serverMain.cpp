#include <iostream>
#include "../../headers/server.hpp"

int main(){
    Server server;
    int ans = 1;
    if(server.setupListenerSocket()){
        server.awaitConnection();
        while(ans != 0){
            if(!server.receiveFromPeer()){
                std::cout << "Connection failed!" << std::endl;
                return 0;
            }
            if(!server.sendAcknowledgement()){
                std::cout << "Could not send acknowledgement!" << std::endl;
                return 0;
            }
            std::cout << "Input 0 if you do not want to receive any more messages. Input 1 if you want to receive more messages." << std::endl;
            std::cin >> ans;
            if(ans == 0){
                std::cout << "Connection will be terminated" << std::endl;

                server.closeConnection();
                break;
            }
        }
    }
    return 0;
}