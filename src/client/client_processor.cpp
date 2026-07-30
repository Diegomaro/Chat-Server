#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <cstring>

#include <iostream>
#include <fcntl.h>

#include <chrono>
#include <thread>

#include "../../headers/client_processor.hpp"

ClientProcessor::ClientProcessor(){
    incoming_buffer_ = new(std::nothrow) uint8_t[config::READING_BUFFER_SIZE];
    outgoing_buffer_ = new(std::nothrow) uint8_t[config::READING_BUFFER_SIZE];
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

bool ClientProcessor::setupClientService(){
    if(!setupHashTables()
    || !setupHeaderTypes()
    || !setupSocket()){
        return false;
    }
    return true;
}

bool ClientProcessor::setupHashTables(){
    if(!username_to_key_.createTable(config::INITIAL_HASHTABLE_SIZE)){
        return false;
    }
    return true;
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

    if(!request_communication_){
        return false;
    }
    request_communication_[0] = UINT8_MAX;
    request_communication_[1] = types::SEND_REQUEST;
    request_communication_[2] = UINT8_MAX;
    request_communication_[3] = UINT8_MAX;
    request_communication_[4] = UINT8_MAX;
    request_communication_[5] = UINT8_MAX;
    request_communication_[6] = 0;
    request_communication_[7] = config::HOSTNAME_LENGTH;

    if(!respond_communication_){
        return false;
    }
    respond_communication_[0] = UINT8_MAX;
    respond_communication_[1] = types::REJECT_REQUEST;
    respond_communication_[2] = UINT8_MAX;
    respond_communication_[3] = UINT8_MAX;
    respond_communication_[4] = UINT8_MAX;
    respond_communication_[5] = UINT8_MAX;
    respond_communication_[6] = 0;
    respond_communication_[7] = config::HOSTNAME_LENGTH;

    if(!auth_message_){
        return false;
    }
    auth_message_[0] = UINT8_MAX;
    auth_message_[1] = UINT8_MAX;
    auth_message_[2] = UINT8_MAX;
    auth_message_[3] = UINT8_MAX;
    auth_message_[4] = UINT8_MAX;
    auth_message_[5] = UINT8_MAX;
    auth_message_[6] = 0;
    auth_message_[7] = 0;

    return true;
}

bool ClientProcessor::setupSocket(){
    int status;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *server_info;
    if((status = getaddrinfo("127.0.0.1", config::SERVER_PORT, &hints, &server_info)) != 0){
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        return false;
    }
    if((client_socket_ = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol)) == -1){
        perror("socket failed");
        return false;
    }
    int yes = 1;
    if (setsockopt(client_socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
        perror("setsockopt SO_REUSEADDR failed");
    }
    if(fcntl(client_socket_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return false;
    }
    if((connect(client_socket_, server_info->ai_addr, server_info->ai_addrlen)) == -1 && errno != EINPROGRESS){
        perror("connect failed");
        return false;
    }
    freeaddrinfo(server_info);
    if ((epoll_fd_ = epoll_create1(0)) == -1){
        perror("epoll failed");
        return false;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_socket_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket_, &ev);
    return true;
}

// Central loop that handles message receiving and sending.
void ClientProcessor::centralLoop(){
    while(program_running_){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_fd_, events_, 10, 1000)) == -1){
            perror("epoll wait failed");
            return;
        }
        for (int i = 0; i < ready_polls; i++){
            if(events_[i].data.fd == client_socket_){
                if (events_[i].events & EPOLLIN){
                    bool receive_loop = true;
                    while(receive_loop){
                        int rcvf_state = receiveFromServer();
                        /*
                        Add receive state that means that the buffer is complete,
                        and requests a checkMessage before erasing the buffer and sending an error message
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
                                    cleanIncomingBuffer();
                                }
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
        if(send_register_){
            int ans = 0;
            switch(ans = sendMessage(credentials_length_ + config::HEADER_SIZE, auth_message_)){
                case status::SUCCESS:{
                    pending_messages_++;  // rework
                    std::cout << "message sent correctly!" << std::endl;
                } break;
                case status::RESOURCE_UNAVAILABLE:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                case status::ERROR:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
            }
            send_register_ = false;
        }
        if(send_request_){
            int ans = 0;
            switch(ans = sendMessage(config::HEADER_SIZE + config::HOSTNAME_LENGTH, request_communication_)){
                case status::SUCCESS:{
                    pending_messages_++; // rework
                    std::cout << "message sent correctly!" << std::endl;
                } break;
                case status::RESOURCE_UNAVAILABLE:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                case status::ERROR:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
            }
            send_request_ = false;
        }
        if(respond_request_){
            int ans = 0;
            switch(ans = sendMessage(config::HEADER_SIZE + config::HOSTNAME_LENGTH, respond_communication_)){
                case status::SUCCESS:{
                    pending_messages_++; // rework
                    std::cout << "message sent correctly!" << std::endl;
                } break;
                case status::RESOURCE_UNAVAILABLE:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                case status::ERROR:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
            }
            respond_request_ = false;
        }
        if(send_message_){
            {
                std::unique_lock<std::mutex> lock_message(read_mutex_);
                int ans = 0;
                switch(ans = sendMessage(msg_len_, outgoing_buffer_)){
                    case status::SUCCESS:{
                        pending_messages_++;
                        std::cout << "message sent correctly!" << std::endl;
                    } break;
                    case status::RESOURCE_UNAVAILABLE:{
                        std::cout << "Could not sent message!" << std::endl;
                    } break;
                    case status::ERROR:{
                        std::cout << "Could not sent message!" << std::endl;
                    } break;
                }
                send_message_ = false;
            }
        }
        if(ready_polls == 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

/*
Sends message from a buffer.
Returns RESOURCE_UNAVAILABLE, ERROR, SUCCESS.
*/
int ClientProcessor::sendMessage(int bytes_to_send, uint8_t *buffer){
    int total_bytes_sent = 0;
    int sent_bytes = 0;

    while(total_bytes_sent < bytes_to_send){
        if((sent_bytes = send(
            client_socket_,
            &buffer[total_bytes_sent],
            bytes_to_send - total_bytes_sent,
            0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of message failed.");
                return status::ERROR;
            }
        }
        total_bytes_sent += sent_bytes;
    }
    return status::SUCCESS;
}

/*
Copies data received from the server to a buffer until there is nothing else to read.
Returns INSUFFICIENT_BUFFER_SPACE, ERROR, CLOSED CONVERSATION, SUCCESS.
*/
int ClientProcessor::receiveFromServer(){
    int total_bytes_received = 0;
    int bytes_received = 0;
    if(byte_counter_ >= config::READING_BUFFER_SIZE){
        return status::INSUFFICIENT_BUFFER_SPACE;
    }
    int bytes_to_copy = config::BUFFER_READING_SIZE;
    if(byte_counter_ + config::BUFFER_READING_SIZE > config::READING_BUFFER_SIZE){
        bytes_to_copy = config::READING_BUFFER_SIZE - byte_counter_;
    }
    if(writing_pointer_ + bytes_to_copy > config::READING_BUFFER_SIZE){
        bytes_to_copy = config::READING_BUFFER_SIZE - writing_pointer_;
    }
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
            return status::CLOSED_CONVERSATION;
            break;
        }
        total_bytes_received += bytes_received;
    }
    if(total_bytes_received == 0){
        return status::NOTHING_TO_READ;
    }
    byte_counter_ += total_bytes_received;
    writing_pointer_ = (writing_pointer_ + total_bytes_received) % config::READING_BUFFER_SIZE;
    return status::SUCCESS;
}

/*
Checks if the entire message header + payload have been received.
Returns INCOMPLETE_MESSAGE, INVALID_MESSAGE, SUCCESS.
*/
int ClientProcessor::checkMessage(){
    if(!valid_header_){
        int header_state = checkHeader();
        if(header_state != status::SUCCESS){
            return header_state;
        }
    }
    // PAYLOAD
    if(byte_counter_ < payload_length_ + config::HEADER_SIZE){
        return status::INCOMPLETE_MESSAGE;
    }
    return status::SUCCESS;
}

/*
Checks if the entire message header have been received.
Returns INCOMPLETE_MESSAGE, INVALID_MESSAGE, SUCCESS.
*/
int ClientProcessor::checkHeader(){
    if(valid_header_){
        return status::SUCCESS;
    }
    if(byte_counter_ < config::HEADER_SIZE){
        return status::INCOMPLETE_MESSAGE;
    }
    reading_pointer_ = starting_pointer_;
    // HEAD_BITS
    if((incoming_buffer_[reading_pointer_] ^ 0xFF) != 0){
        return status::INVALID_MESSAGE;
    }
    advanceReadingPointer();
    // TYPE
    if(type_ == types::INVALID_TYPE){
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
    valid_header_ = true;
    return status::SUCCESS;
}

/*
Returns INVALID_MESSAGE, RESOURCE_UNAVAILABLE, ERROR, SUCCESS.
*/
int ClientProcessor::actOnMessage(){
    switch(type_){
        case types::USER:{
            if(payload_length_ == 0 || payload_length_ > config::MAX_MESSAGE_SIZE){
                return status::INVALID_MESSAGE;
            }
            if(!printMessage()){ // should not just print, handle later
                return status::ERROR;
            }
            for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
                ack_message_[i + 2] = sender_key_ >> ((config::CLIENT_KEY_LENGTH - i - 1) * 8);
            }
            uint8_t ack_state = sendMessage(config::HEADER_SIZE, ack_message_);
            switch(ack_state){
                case status::RESOURCE_UNAVAILABLE:{
                    // should not return, rather be stored
                    return status::RESOURCE_UNAVAILABLE;
                } break;
                case status::ERROR:{
                    return status::ERROR;
                } break;
            }
        } break;
        case types::REGISTER:{
            uint8_t register_type = incoming_buffer_[reading_pointer_];
            advanceReadingPointer();
            switch(register_type){
                case auth::VALID:{
                    std::cout << "Logged in!" << std::endl;
                    logged_in_ = true;
                } break;
                case auth::INVALID_CREDENTIAL:{
                    std::cout << "Invalid credentials. Please try again!" << std::endl;
                } break;
                case auth::NOT_UNIQUE:{
                    std::cout << "\"" << username_ << "\" is not available!" << std::endl;
                } break;
                case auth::ALREADY_LOGGED_IN:{
                    std::cout << "Already logged in!" << std::endl;
                } break;
            }
        } break;
        case types::LOGIN:{
            // implement much later
        } break;
        case types::SEND_REQUEST:
        case types::REJECT_REQUEST:
        case types::ACCEPT_REQUEST:{
            if(payload_length_ != config::HOSTNAME_LENGTH){
                return status::INVALID_MESSAGE;
            }
            std::string temp_username(config::HOSTNAME_LENGTH, '\0');
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                temp_username[i] = incoming_buffer_[reading_pointer_];
                advanceReadingPointer();
            }
            if(!validateCredential(temp_username, config::HOSTNAME_LENGTH, config::HOSTNAME_LENGTH)){
                std::cout << "Invalid username received!" << std::endl;
                return status::INVALID_CLIENT;
            }
            int temp_key = UINT32_MAX;
            if((temp_key = getUserKey(temp_username)) != UINT32_MAX){
                std::cout << "already known client" << std::endl;
                return status::INVALID_CLIENT;
            } else{
                if(type_ == types::ACCEPT_REQUEST){
                    if(!addUser(sender_key_, temp_username)){
                        return status::ERROR;
                    }
                } else if(type_ == types::SEND_REQUEST){
                    UsernameMapping usernameMapping;
                    usernameMapping.key = sender_key_;
                    for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                        usernameMapping.username[i] = temp_username[i];
                    }
                    if(!incoming_requests_.insertTail(usernameMapping)){
                        return status::ERROR;
                    }
                }
                // remove from outgoing requests
            }
        } break;
        case types::ACK:{
            if(sender_key_ == UINT32_MAX){
                pending_messages_--;
                // handle later :)
                //std::cout << "pending ack: " << pending_messages_ << std::endl;
            } else{
                char *user = getUserFromKey(sender_key_);
                if(user == nullptr){
                    return status::INVALID_CLIENT;
                }
                std::cout << "Message to " << user << " has been delivered!" << std::endl;
            }
        } break;
        default:{
            return status::INVALID_MESSAGE;
        }
    }
    return status::SUCCESS;
}

// Advances reading pointer by 1 or resets to 0 if border is reached.
void ClientProcessor::advanceReadingPointer(){
    if(reading_pointer_ + 1 >= config::READING_BUFFER_SIZE){
        reading_pointer_ = 0;
    } else{
        reading_pointer_++;
    }
}

// Prints message received from other clients, the sender and receiver are printed as well.
bool ClientProcessor::printMessage(){
    char *temp_username;
    if((temp_username = getUserFromKey(sender_key_)) == nullptr){
        return false;
    }
    std::cout << std::endl;
    for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
        if(temp_username[i] == '\0'){
            break;
        }
        std::cout << temp_username[i];
    }
    std::cout << " -> " << username_ << ": ";
    for(int i = 0; i < payload_length_; i++){
        std::cout << static_cast<char>(incoming_buffer_[reading_pointer_]);
        advanceReadingPointer();
    }
    std::cout << std::endl;
    return true;
}

// Resets values to prepare to receive new messages.
void ClientProcessor::cleanIncomingBuffer(){
    valid_header_ = false;
    starting_pointer_ = reading_pointer_;
    byte_counter_ -= (payload_length_ + config::HEADER_SIZE);
    payload_length_ = UINT16_MAX;
    type_ =types::INVALID_TYPE;
    sender_key_ = UINT32_MAX;
}

// Central loop that handles login/register and user input to communicate to other clients.
void ClientProcessor::inputLoop(){
    if(!welcomeInputLoop()){
        return;
    }
    if(!messageInputLoop()){
        return;
    }
}

// Handles login and register of users.
bool ClientProcessor::welcomeInputLoop(){
    bool welcome_program_running_ = true;
    int ans = -1;
    while(welcome_program_running_){
        std::cout
            << "Welcome Menu." << std::endl
            << "1. Login. (implement later)" << std::endl
            << "2. Register." << std::endl
            << "3. Enter Main Menu." << std::endl
            << "4. Exit." << std::endl;
        ans = validateInputIsNumeric();
        switch(ans){
            case 1:{
            } break;
            case 2:{
                std::string tmp_username;
                std::cout << "Choose a username. It must only contain letters, numbers and underscores (_)."
                << std::endl << "The maximum size is "
                << static_cast<uint>(config::HOSTNAME_LENGTH) << " characters."
                << std::endl << "Username: ";
                std::getline(std::cin, tmp_username);
                if(!validateCredential(tmp_username, 1, config::HOSTNAME_LENGTH)){
                    std::cout << "Try a different username!"  << std::endl;
                    break;
                }
                std::string tmp_password;
                std::cout << "Choose a password. It must only contain letters, numbers and underscores (_)."
                << std::endl << "The minimum size is "
                << static_cast<uint>(config::MIN_PASSWORD_LENGTH) << " characters."
                << std::endl << "The maximum size is "
                << static_cast<uint>(config::MAX_PASSWORD_LENGTH) << " characters."
                << std::endl << "Password: ";
                std::getline(std::cin, tmp_password);
                if(!validateCredential(tmp_password, config::MIN_PASSWORD_LENGTH, config::MAX_PASSWORD_LENGTH)){
                    break;
                }

                std::cout << "Adequate credentials!" << std::endl;

                username_ = tmp_username;
                password_ = tmp_password;
                credentials_length_ = config::HOSTNAME_LENGTH + password_.length();

                auth_message_[1] = types::REGISTER;
                auth_message_[7] = credentials_length_;
                for(int i = 0; i < username_.length(); i++){
                    auth_message_[i + config::HEADER_SIZE] = username_[i];
                }
                for(int i = username_.length(); i < config::HOSTNAME_LENGTH; i++){
                    auth_message_[i + config::HEADER_SIZE] = 0;
                }
                for(int i = 0; i < password_.length(); i++){
                    auth_message_[i + config::HOSTNAME_LENGTH + config::HEADER_SIZE] = password_[i];
                }

                for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                    respond_communication_[i + config::HEADER_SIZE] = username_[i];
                }
                send_register_ = true;
            } break;
            case 3:{
                if(logged_in_){
                    welcome_program_running_ = false;
                } else{
                    std::cout << "You must register or login first!"  << std::endl;
                }
            } break;
            case 4:{
                welcome_program_running_ = false;
                program_running_ = false;
                return false;
            } break;
        }
    }
    return true;
}

// Validates if a credential contains valid characters and is of allowed size.
bool ClientProcessor::validateCredential(const std::string &credential, uint8_t min_length, uint8_t max_length){
    if(credential.size() < min_length || credential.size() > max_length){
        std::cout << "Credential is too long or too short!"  << std::endl;
        return false;
    }
    bool valid_credential = true;
    for(int i = 0; i < credential.size(); i++){
        if((credential[i] > 0 && credential[i] < 48)
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

// Message loop to handle user messaging.
bool ClientProcessor::messageInputLoop(){
    int main_ans = -1;
    while(program_running_){
        std::cout << "Main Menu." << std::endl
            << "1. Set message." << std::endl
            << "2. Set destinatory. (" << receiving_username_ << ")" << std::endl
            << "3. Send message." << std::endl
            << "4. Send request." << std::endl
            << "5. Manage requests. (" << static_cast<uint>(incoming_requests_.getSize()) << ")" << std::endl
            << "6. Reload." << std::endl
            << "7. Exit." << std::endl;
        main_ans = validateInputIsNumeric();
        int result = 0;
        switch(main_ans){
            case 1:{
            result = setMessage();
            switch(result){
                case status::SUCCESS:{
                    std::cout << "Message set correctly!" << std::endl;
                } break;
                case status::INVALID_MESSAGE:{
                    std::cout << "Invalid message, please try again!" << std::endl;
                } break;
                case status::ERROR:{
                    return false;
                }
            }
            } break;
            case 2:{
                result = setReceiver();
                switch(result){
                    case status::SUCCESS:{
                        std::cout << "Receiver key set correctly!" << std::endl;
                    } break;
                    case status::INVALID_MESSAGE:{
                        std::cout << "Invalid client, please try again!" << std::endl;
                    } break;
                    case status::ERROR:{
                        return false;
                    }
                }
            } break;
            case 3:{
                if(receiver_key_ == UINT32_MAX || message_.length() == 0 || message_.length() > config::MAX_MESSAGE_SIZE){
                    std::cout << "Please set a valid receiver key and message first!" << std::endl;
                } else{
                    send_message_ = true;
                }
            } break;
            case 4:{
                std::string temp_username(config::HOSTNAME_LENGTH, '\0');
                std::cout << "Input the username of the user you want to establish a communication with: ";
                std::getline(std::cin, temp_username);
                if(validateCredential(temp_username, 1, config::HOSTNAME_LENGTH)){
                    for(int i = 0; i < temp_username.length(); i++){
                        request_communication_[i + config::HEADER_SIZE] = temp_username[i];
                    }
                    for(int i = temp_username.length(); i < config::HOSTNAME_LENGTH; i++){
                        request_communication_[i + config::HEADER_SIZE] = 0;
                    }
                    send_request_ = true;
                } else{
                    break;
                }
            } break;
            case 5:{
                if(incoming_requests_.isEmpty()){
                    std::cout << "No requests available!" << std::endl;
                    break;
                }
                std::cout << "Input a number corresponding to a request to decide what to do with it. Input 0 to exit." << std::endl;
                std::cout << "Requests: " << std::endl;
                incoming_requests_.resetNodeIndex();
                int ctr = 1;
                while(incoming_requests_.hasNode()){
                    std::cout << ctr++ << ": " << incoming_requests_.getNode().username << std::endl;
                    incoming_requests_.advanceNode();
                }
                int ans = validateInputIsNumeric();
                if(ans == 0){
                    break;
                }
                if(ans < 0 || ans > ctr){
                    std::cout << "Invalid request selected!" << std::endl;
                     break;
                }
                incoming_requests_.resetNodeIndex();
                for(int i = 0; i < ans - 1; i++){
                    if(!incoming_requests_.hasNode()){
                        break;
                    }
                    incoming_requests_.advanceNode();
                }
                std::cout << "Select one option." << std::endl
                << "Request from : " << incoming_requests_.getNode().username << std::endl
                << "1. Accept" << std::endl
                << "2. Reject" << std::endl
                << "3. Exit" << std::endl;
                ans = validateInputIsNumeric();
                if(ans == 1 || ans == 2){
                    char *ref_username = incoming_requests_.getNode().username;
                    UsernameMapping usernameMapping;
                    usernameMapping.key = incoming_requests_.getNode().key;
                    for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                        usernameMapping.username[i] = ref_username[i];
                    }
                    if(ans == 1){
                        if(!username_to_key_.insertNode(stringHash(incoming_requests_.getNode().username), usernameMapping)){
                            return false;
                        }
                        respond_communication_[1] = types::ACCEPT_REQUEST;
                    } else{
                        respond_communication_[1] = types::REJECT_REQUEST;
                    }
                    if(!incoming_requests_.deleteNode(usernameMapping)){
                        return false;
                    }
                    for(int i = 0; i < 4; i++){
                        respond_communication_[i + 2] = usernameMapping.key << ((3 - i) * 8);
                    }
                    respond_request_ = true;
                }
            } break;
            case 6:{
            } break;
            case 7:{
                program_running_ = false;
                return false;
            } break;
            default:{
                std::cout << "Incorrect input!" << std::endl;
            }
        }
    }
    return true;
}

/*
Sets message to send in a buffer.
Returns INVALID_MESSAGE, SUCCESS.
*/
int ClientProcessor::setMessage(){
    if(!outgoing_buffer_){
        return status::ERROR;
    }
    std::cout << "Message: ";
    std::getline(std::cin, message_);

    if(message_.length() == 0 || message_.length() > config::MAX_MESSAGE_SIZE){
        return status::INVALID_MESSAGE;
    }

    {
        std::unique_lock<std::mutex> lock_message(read_mutex_);
        uint16_t message_length = message_.length();

        outgoing_buffer_[0] = 255;
        outgoing_buffer_[1] = types::USER;
        outgoing_buffer_[6] = message_length >> 8;
        outgoing_buffer_[7] = message_length;


        for(int i = 0; i < message_length; i++){
            outgoing_buffer_[8 + i] = message_[i];
        }
        msg_len_ = 8 + message_length;
    }
    return status::SUCCESS;
}

/*
Sets destinatory from a list of known users.
REturns ERROR, NOTHING_TO_DO, INVALID_CLIENT, SUCCESS.
*/
int ClientProcessor::setReceiver(){
    if(!outgoing_buffer_){
        return status::ERROR;
    }
    int ctr = 1;
    if(username_to_key_.getDataCount() == 0){
        std::cout << "No known users, request a user to establish a connection first!" << std::endl;
        return status::NOTHING_TO_DO;
    }
    std::cout << "Please input the destinatory username. " << std::endl
    << "Known users: " << std::endl;
    username_to_key_.resetNodeIndex();
    while(username_to_key_.hasNodes()){
        if(username_to_key_.hasNode()){
            std::cout  << ctr++ << ": " << username_to_key_.getNode()->data_.username << std::endl;
        }
        username_to_key_.advanceNode();
    }

    std::string temp_username(config::HOSTNAME_LENGTH, '\0');
    std::getline(std::cin, temp_username);
    uint32_t temp_key;
    if((temp_key = getUserKey(temp_username)) == UINT32_MAX){
        std::cout << "Invalid username!" << std::endl;
        return status::INVALID_CLIENT;
    }
    receiver_key_ = temp_key;
    receiving_username_ = temp_username;

    outgoing_buffer_[2] = receiver_key_ >> 24;
    outgoing_buffer_[3] = receiver_key_ >> 16;
    outgoing_buffer_[4] = receiver_key_ >> 8;
    outgoing_buffer_[5] = receiver_key_;
    return status::SUCCESS;
}

// Add a user to the "list" of known users.
bool ClientProcessor::addUser(uint32_t key, const std::string &username){
    UsernameMapping user;
    user.key = key;
    std::memcpy(user.username, username.data(), config::HOSTNAME_LENGTH);
    unsigned long hash_key = stringHash(user.username);
    if(!username_to_key_.insertNode(hash_key, user)){
        return false;
    }
    return true;
}

// Gets a key corresponding to a specific user. Returns UINT32_MAX if the user does not exist.
uint32_t ClientProcessor::getUserKey(const std::string &temp_username){
    char username [config::HOSTNAME_LENGTH];
    std::strcpy(username, temp_username.c_str());
    unsigned long hash_key = stringHash(username);
    if(!username_to_key_.searchNode(hash_key)){
        return UINT32_MAX;
    }
    username_to_key_.resetNodeIndex();
    while(username_to_key_.hasNodes()){
        if(username_to_key_.hasNode()){
            bool equal_usernames = true;
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                if(username_to_key_.getNode()->data_.username[i] != temp_username[i]){
                    equal_usernames = false;
                    break;
                }
            }
            if(equal_usernames){
                return username_to_key_.getNode()->data_.key;
            }
        }
        username_to_key_.advanceNode();
    }
    return UINT32_MAX;
}

// Gets a user's username from their key. Returns nullptr if the user does not exist.
char* ClientProcessor::getUserFromKey(uint32_t key){
    username_to_key_.resetNodeIndex();
    while(username_to_key_.hasNodes()){
        if(username_to_key_.hasNode()){
            if(username_to_key_.getNode()->data_.key == key){
                return username_to_key_.getNode()->data_.username;
            }
        }
        username_to_key_.advanceNode();
    }
    return nullptr;
}

int ClientProcessor::validateInputIsNumeric(){
    std::string ans;
    int parsed_ans = -1;
    while(parsed_ans == -1){
        if(integerCheck(ans)){
            if(ans.length() < config::MAX_LENGTH_OF_INT_CHOICE){
                parsed_ans = std::stoi(ans);
            } else{
            std::cout << "Invalid input, please enter a number from the selection." << std::endl;
            }
        } else{
            std::cout << "Invalid input, please enter a number." << std::endl;
        }
    }
    return parsed_ans;
}

bool ClientProcessor::integerCheck(const std::string &string){
    for(int i = 0; i < (int)string.size(); i++){
        if(string[i] == '0' || string[i] == '1' ||
            string[i] == '2' || string[i] == '3' ||
            string[i] == '4' || string[i] == '5' ||
            string[i] == '6' || string[i] == '7' ||
            string[i] == '8' || string[i] == '9'){
        } else {
            return false;
        }
    }
    return true;
}

unsigned long ClientProcessor::stringHash(const char *str){
    unsigned long hash = 5381;
    int c;
    while (c = *str++){
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}