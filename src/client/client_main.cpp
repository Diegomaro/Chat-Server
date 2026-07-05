#include <iostream>
#include <string>
#include <limits>
#include <thread>

#include "../../headers/client_processor.hpp"

int main(){
    ClientProcessor clientProcessor;
    if(clientProcessor.setupSocket()){
        std::thread central_thread(&ClientProcessor::centralLoop, &clientProcessor);
        std::thread input_thread(&ClientProcessor::messageInputLoop, &clientProcessor);
        central_thread.join();
        input_thread.join();
    }
    return 0;
}