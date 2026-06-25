#include "../../headers/client.hpp"

int main(){
    Client client;
    if(client.setupSocket()){
        client.sendMessage();
        client.receiveFromServer();
    }
    return 0;
}