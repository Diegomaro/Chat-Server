#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#include <iostream>
#include <fcntl.h>

#include <chrono>
#include <thread>

#include "../../headers/client_processor.hpp"

ClientProcessor::ClientProcessor(){
    memset(&hints_, 0, sizeof(hints_));
    hints_.ai_family = AF_UNSPEC;
    hints_.ai_socktype = SOCK_STREAM;

    client_socket_ = -1;
    epoll_fd_ = -1;

    incoming_buffer_ = new(std::nothrow) uint8_t[config::READING_BUFFER_SIZE];
    outgoing_buffer_ = new(std::nothrow) uint8_t[config::READING_BUFFER_SIZE];

    msg_len_ = 1024;

    starting_pointer_ = 0;
    writing_pointer_ = 0;
    reading_pointer_ = 0;

    byte_counter_ = 0;
    payload_length_ = UINT16_MAX;
    type_ = 0;
    sender_key_ = UINT32_MAX;
    receiver_key_ = UINT32_MAX;

    pending_messages = 0;

    requests_ = 0; // load requests from file later
}

ClientProcessor::~ClientProcessor(){
    if(incoming_buffer_){
        delete [] incoming_buffer_;
        incoming_buffer_ = nullptr;
    }
    if(outgoing_buffer_){
        delete [] outgoing_buffer_;
        outgoing_buffer_ = nullptr;
    }
    close(client_socket_);
}

bool ClientProcessor::setupHeaderTypes(){
    if(!ack_message_){
        return false;
    }
    ack_message_[0] = UINT8_MAX;
    ack_message_[1] = types::ACK;
    ack_message_[2] = UINT8_MAX;
    ack_message_[3] = UINT8_MAX;
    ack_message_[4] = UINT8_MAX;
    ack_message_[5] = UINT8_MAX;
    ack_message_[6] = 0;
    ack_message_[7] = 0;
    return true;
}

bool ClientProcessor::setupSocket(){
    int status;
    if((status = getaddrinfo("127.0.0.1", config::SERVER_PORT, &hints_, &server_info_)) != 0){
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        return false;
    }
    if((client_socket_ = socket(server_info_->ai_family, server_info_->ai_socktype, server_info_->ai_protocol)) == -1){
        perror("socket failed");
        return false;
    }
    int yes = 1;
    if (setsockopt(client_socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt SO_REUSEADDR failed");
    }
    if(fcntl(client_socket_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return false;
    }
    if((connect(client_socket_, server_info_->ai_addr, server_info_->ai_addrlen)) == -1 && errno != EINPROGRESS){
        perror("connect failed");
        return false;
    }
    freeaddrinfo(server_info_);
    if ((epoll_fd_ = epoll_create1(0)) == -1) {
        perror("epoll failed");
        return false;
    }
    ev_.events = EPOLLIN | EPOLLET;
    ev_.data.fd = client_socket_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket_, &ev_);
    return true;
}

void ClientProcessor::centralLoop(){
    while(program_running_){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_fd_, events_, 10, 1000)) == -1){
            perror("epoll wait failed");
            return;
        }
        for (int i = 0; i < ready_polls; i++) {
            if(events_[i].data.fd == client_socket_){
                if (events_[i].events & EPOLLIN) {
                    bool receive_loop = true;
                    while(receive_loop){
                        int rcvf_state = receiveFromServer();
                        /*
                        Add receive state that means that the buffer is complete,
                        and requests a verify message before erasing the buffer and sending an error message
                        */
                        switch(rcvf_state){
                            case status::SUCCESS:{
                                while(checkMessage() == status::SUCCESS){
                                    int check_state = actOnMessage();
                                    switch(check_state){
                                        case status::INVALID_MESSAGE:{
                                            // handle later
                                        } break;
                                        case status::RESOURCE_UNAVAILABLE:{
                                            // handle later
                                        } break;
                                        case status::ERROR:{
                                            return;
                                        } break;
                                    }
                                    if(!cleanIncomingBuffer()){
                                        return;
                                    }
                                }
                                receive_loop = false;
                            //if missing timeout
                            } break;
                            case status::NOTHING_TO_READ:{
                                receive_loop = false;
                            } break;
                            case status::ERROR:{
                                return;
                            } break;
                            case status::CLOSED_CONVERSATION:{
                                return;
                            } break;
                            case status::INSUFFICIENT_BUFFER_SPACE:{
                                //send fail message to server
                                byte_counter_ = 0;
                                starting_pointer_ = 0;
                                reading_pointer_ = 0;
                                writing_pointer_ = 0;
                                sender_key_ = UINT32_MAX;
                                type_ = 0;
                                payload_length_ = UINT16_MAX;
                                receive_loop = false;
                            } break;
                        }
                    }
                }
            }
        }
        if(send_message_){
            {
                std::unique_lock<std::mutex> lock_message(read_mutex_);
                int ans = 0;
                switch(ans = sendMessage()){
                    case status::SUCCESS:{
                        pending_messages++;
                        std::cout << "message sent correctly!" << std::endl;
                    }break;
                    case status::NOTHING_TO_READ:{
                        std::cout << "Could not sent message!" << std::endl;
                    }break;
                    case status::ERROR:{
                        std::cout << "Could not sent message!" << std::endl;
                    }break;
                }
                send_message_ = false;
            }
        }
        if(ready_polls == 0){
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

int ClientProcessor::sendMessage(){
    int bytes_to_send = msg_len_;
    int bytes_sent = 0;

    while(bytes_sent < bytes_to_send){
        int sent_bytes = 0;
        if((sent_bytes = send(
            client_socket_,
            &outgoing_buffer_[bytes_sent],
            (bytes_to_send - bytes_sent),
            0)) == -1)
        {
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return status::NOTHING_TO_READ; // change to nothing to write
            } else{
                perror("Send of message failed.");
                return status::ERROR;
            }
        } else{
            bytes_sent += sent_bytes;
        }
    }
    return status::SUCCESS;
}

int ClientProcessor::sendAcknowledgement(){
    int total_bytes_sent = 0;
    int bytes_sent = 0;

    ack_message_[2] = sender_key_ >> 24;
    ack_message_[3] = sender_key_ >> 16;
    ack_message_[4] = sender_key_ >> 8;
    ack_message_[5] = sender_key_;

    while(total_bytes_sent < config::HEADER_SIZE){
        if((bytes_sent = send(client_socket_, &ack_message_[total_bytes_sent], config::HEADER_SIZE - total_bytes_sent, 0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of acknowledgement failed.");
                return status::ERROR;
            }
        }
        total_bytes_sent += bytes_sent;
    }
    return status::SUCCESS;
}

int ClientProcessor::receiveFromServer(){
    int total_bytes_received = 0;
    int bytes_received = 0;
    if(byte_counter_ + config::BUFFER_READING_SIZE > config::READING_BUFFER_SIZE){
        return status::INSUFFICIENT_BUFFER_SPACE;
    }

    int reached_buffer_limit = false;
    int bytes_to_copy = config::BUFFER_READING_SIZE;
    if(writing_pointer_ + config::BUFFER_READING_SIZE > config::READING_BUFFER_SIZE){
        bytes_to_copy = config::READING_BUFFER_SIZE - writing_pointer_;
        reached_buffer_limit = true;
    }

    //byte counter
    while(total_bytes_received < bytes_to_copy){
        if((bytes_received = recv(
            client_socket_,
            &incoming_buffer_[writing_pointer_ + total_bytes_received],
            bytes_to_copy - total_bytes_received,
            0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                break;
            } else{
                perror("An error ocurred while receiving from client.");
                return status::ERROR;
            }
        } else if(bytes_received == 0){
            break;
        }
        total_bytes_received += bytes_received;
    }
    if(total_bytes_received == 0){
        return status::CLOSED_CONVERSATION;
    }

    byte_counter_ += total_bytes_received;
    writing_pointer_ = (writing_pointer_ + total_bytes_received) % config::READING_BUFFER_SIZE;
    return status::SUCCESS;
}

int ClientProcessor::checkMessage(){
    // HEAD_BITS
    if(8 > byte_counter_){
        return status::INVALID_MESSAGE;
    }
    reading_pointer_ = starting_pointer_;
    if((incoming_buffer_[reading_pointer_] ^ 0xFF) != 0){
        return status::INVALID_MESSAGE;
    }
    advanceReadingPointer();

    // TYPE
    if(type_ == 0){
        type_ = incoming_buffer_[reading_pointer_];
    }
    advanceReadingPointer();
    // HOST_KEY
    if(sender_key_ == UINT32_MAX){
        sender_key_ = 0;
        for(int i = 0; i < 4; i++){
            sender_key_ = sender_key_ | (incoming_buffer_[reading_pointer_]) << ((config::CLIENT_KEY_LENGTH - 1 - i) * 8);
            advanceReadingPointer();
        }
    } else{
        for(int i = 0; i < 4; i++){
            advanceReadingPointer();
        }
    }

    // PAYLOAD_LENGTH
    if(payload_length_ == UINT16_MAX){
        payload_length_ = 0;
        payload_length_ = incoming_buffer_[reading_pointer_] << 8;
        advanceReadingPointer();
        payload_length_ = payload_length_ | (incoming_buffer_[reading_pointer_]);
    } else{
        advanceReadingPointer();
    }
    advanceReadingPointer();
    if(byte_counter_ < payload_length_ + config::HEADER_SIZE){
        return status::INCOMPLETE_MESSAGE;
    }
    return status::SUCCESS;
}

/*
Returns INVALID_MESSAGE, RESOURCE_UNAVAILABLE, ERROR, SUCCESS
*/
int ClientProcessor::actOnMessage(){
    switch(type_){
        case types::USER:{
            if(payload_length_ == 0 || payload_length_ > config::MAX_MESSAGE_SIZE){
                return status::INVALID_MESSAGE;
            }
            if(!printMessage()){
                return status::ERROR;
            }

            uint8_t ack_state = sendAcknowledgement();
            switch(ack_state){
                case status::RESOURCE_UNAVAILABLE:{
                    // should not return, rather be stored
                    return status::RESOURCE_UNAVAILABLE;
                } break;
                case status::ERROR:{
                    return status::ERROR;
                } break;
            }

            return status::SUCCESS;
        } break;
        case types::GROUP:{
            //implement much later
        } break;
        case types::AUTH_KEY:{
            // implement much later
        } break;
        case types::SEND_REQUEST:{
            // implement much later
        } break;
        case types::ACCEPT_REQUEST:{
            // implement much later
        } break;
        case types::ACK:{
            if(sender_key_ == UINT32_MAX){
                pending_messages--;
                // handle later :)
                //std::cout << "pending ack: " << pending_messages << std::endl;
            } else{
                std::cout << "Message to " << sender_key_ << " has been delivered!" << std::endl;
            }
        } break;
        default:{
            return status::INVALID_MESSAGE;
        }
    }
    return status::SUCCESS;
}

void ClientProcessor::advanceReadingPointer(){
    if(reading_pointer_ + 1 >= config::READING_BUFFER_SIZE){
        reading_pointer_ = 0;
    } else{
        reading_pointer_++;
    }
}

bool ClientProcessor::printMessage(){
    std::cout << std::endl << static_cast<uint>(sender_key_) << " -> Me: ";
    for(int i = 0; i < payload_length_; i++){
        std::cout << static_cast<char>(incoming_buffer_[reading_pointer_]);
        advanceReadingPointer();
    }
    std::cout << std::endl;
    return true;
}

bool ClientProcessor::cleanIncomingBuffer(){
    starting_pointer_ = reading_pointer_;
    byte_counter_ -= (payload_length_ + config::HEADER_SIZE);
    type_ = 0;
    payload_length_ = UINT16_MAX;
    sender_key_ = UINT32_MAX;
    return true;
}

void ClientProcessor::inputLoop(){
    if(!welcomeInputLoop()){
        return;
    }
    if(!messageInputLoop()){
        return;
    }
}

bool ClientProcessor::welcomeInputLoop(){
    bool welcome_program_running_ = true;
    int welcome_ans = 0;
    while(welcome_program_running_){
        std::cout << "Welcome Menu." << std::endl
        << "1. Login. (implement later)" << std::endl
        << "2. Register." << std::endl // right now only set username.
        << "3. Exit." << std::endl;
        std::cin >> welcome_ans;
        switch(welcome_ans){
            case 1:{
            } break;
            case 2:{
                std::string tmp_username;
                std::cout << "Choose a username. It must only contain letters, numbers and underscores (_)."
                << std::endl << "The maximum size is "
                << static_cast<uint>(config::HOSTNAME_LENGTH) << " characters."
                << std::endl << "Username: ";
                std::getline(std::cin >> std::ws, tmp_username);
                if(validateCredential(tmp_username, 1, config::HOSTNAME_LENGTH)){
                    username_ = tmp_username;
                    std::cout << "username set!" << std::endl;
                } else{
                    break;
                }

                std::string tmp_password;
                std::cout << "Choose a password. It must only contain letters, numbers and underscores (_)."
                << std::endl << "The minimum size is "
                << static_cast<uint>(config::MIN_PASSWORD_LENGTH) << " characters."
                << std::endl << "The maximum size is "
                << static_cast<uint>(config::MAX_PASSWORD_LENGTH) << " characters."
                << std::endl << "Password: ";
                std::getline(std::cin >> std::ws, tmp_password);

                if(validateCredential(tmp_password, config::MIN_PASSWORD_LENGTH, config::MAX_PASSWORD_LENGTH)){
                    password_ = tmp_password;
                    std::cout << "Password set!" << std::endl;
                } else{
                    break;
                }
                welcome_program_running_ = false;
            } break;
            case 3:{
                welcome_program_running_ = false;
                program_running_ = false;
                return false;
            } break;
        }
    }
    return true;
}

bool ClientProcessor::validateCredential(std::string &credential, uint8_t min_length, uint8_t max_length){
    if(credential.size() < min_length || credential.size() > max_length){
        std::cout << "Credential is too long or too short!"  << std::endl;
        return false;
    }
    bool valid_credential = true;
    for(int i = 0; i < credential.size(); i++){
        if(credential[i] < 48
            || (credential[i] > 57 && credential[i] < 65)
            || (credential[i] > 90 && credential[i] < 95)
            || (credential[i] > 95 && credential[i] < 97)
            || credential[i] > 122){
                std::cout << "Invalid character: " << credential[i] << std::endl;
                valid_credential = false;
                break;
        }
    }
    if(valid_credential){
        return true;
    }
    return false;
}

bool ClientProcessor::messageInputLoop(){
    int main_ans = 0;
    while(program_running_){
        std::cout << "Main Menu." << std::endl
        << "1. Set message." << std::endl
        << "2. Set destinatory. (" << static_cast<uint>(receiver_key_) << ")" << std::endl // later on, username instead of receiver key
        << "3. Send message." << std::endl
        << "4. Send request." << std::endl
        << "5. Manage requests. (" << static_cast<uint>(requests_) << ")" << std::endl
        << "6. Reload." << std::endl
        << "7. Exit." << std::endl
        << ":: ";
        std::cin >> main_ans;
        int result = 0;
        switch(main_ans){
            case 1:{
            result = setMessage();
            switch(result){
                case status::SUCCESS:{
                    std::cout << "Message set correctly!" << std::endl;
                }break;
                case status::INVALID_MESSAGE:{
                    std::cout << "Invalid message, please try again!" << std::endl;
                }break;
                case status::ERROR:{
                    return false;
                }
            }
            }break;
            case 2:{
                result = setDestinatory();
                switch(result){
                    case status::SUCCESS:{
                        std::cout << "Receiver key set correctly!" << std::endl;
                    }break;
                    case status::INVALID_MESSAGE:{
                        std::cout << "Invalid client, please try again!" << std::endl;
                    }break;
                    case status::ERROR:{
                        return;
                    }
                }
            }break;
            case 3:{
                if(receiver_key_ > 10 || message_.length() == 0 || message_.length() > config::MAX_MESSAGE_SIZE){
                    std::cout << "Please set a valid receiver key and message first!" << std::endl;
                } else{
                    send_message_ = true;
                }
            }break;
            case 4:{
            }break;
            case 5:{
            }break;
            case 6:{
            }break;
            case 7:{
                program_running_ = false;
                return false;
            }break;
            default:{
                std::cout << "Error: Incorrect input!" << std::endl;
            }
        }
    }
    return true;
}

int ClientProcessor::setMessage(){
    if(!outgoing_buffer_){
        return status::ERROR;
    }
    std::cout << "Message: ";
    std::getline(std::cin >> std::ws, message_);

    if(message_.length() == 0 || message_.length() > config::MAX_MESSAGE_SIZE){
        return status::INVALID_MESSAGE;
    }

    {
        std::unique_lock<std::mutex> lock_message(read_mutex_);
        uint16_t message_length = message_.length();

        outgoing_buffer_[0] = 255;
        outgoing_buffer_[1] = types::USER;
        outgoing_buffer_[2] = receiver_key_ >> 24; // remove these
        outgoing_buffer_[3] = receiver_key_ >> 16;
        outgoing_buffer_[4] = receiver_key_ >> 8;
        outgoing_buffer_[5] = receiver_key_;
        outgoing_buffer_[6] = message_length >> 8;
        outgoing_buffer_[7] = message_length;


        for(int i = 0; i < message_length; i++){
            outgoing_buffer_[8 + i] = message_[i];
        }
        msg_len_ = 8 + message_length;
    }
    return status::SUCCESS;
}

int ClientProcessor::setDestinatory(){
    if(!outgoing_buffer_){
        return status::ERROR;
    }
    std::cout << "Destinatory (number between 0 and 10 [temporary]): ";
    std::cin >> receiver_key_;

    if(receiver_key_ > 10){
        return status::INVALID_CLIENT;
    }
    outgoing_buffer_[2] = receiver_key_ >> 24;
    outgoing_buffer_[3] = receiver_key_ >> 16;
    outgoing_buffer_[4] = receiver_key_ >> 8;
    outgoing_buffer_[5] = receiver_key_;
    return status::SUCCESS;
}