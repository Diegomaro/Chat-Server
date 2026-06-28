#include <iostream>
#include <string>
#include <limits>
#include "../../headers/client_processor.hpp"

int main(){
    ClientProcessor clientProcessor;
    int ans = 1;
    std::string msg;
    if(clientProcessor.setupSocket()){
        while(ans != 0){
            do{
                std::cout << "Input message to be sent." << std::endl;
                std::getline(std::cin, msg);
            } while(!clientProcessor.setMessage(msg));

            clientProcessor.sendMessage();
            clientProcessor.receiveFromServer();
            std::cout << "Input 0 if you do not want to send any more messages. Input 1 if you want to send more messages." << std::endl;
            std::cin >> ans;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if(ans == 0){
                std::cout << "Connection will be terminated" << std::endl;
                break;
            }
        }
    }
    return 0;
}