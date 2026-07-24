#include <iostream>
#include <string>
#include <limits>
#include <thread>

#include "../../headers/client_processor.hpp"

int main(){
    ClientProcessor clientProcessor;
    if(!clientProcessor.setupHeaderTypes() || !clientProcessor.setupHashmap()){
        return 1;
    }
    if(clientProcessor.setupSocket()){
        std::thread central_thread(&ClientProcessor::centralLoop, &clientProcessor);
        std::thread input_thread(&ClientProcessor::inputLoop, &clientProcessor);
        central_thread.join();
        input_thread.join();
    }
    return 0;
}