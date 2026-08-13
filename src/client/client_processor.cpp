#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <cstring>

#include <iostream>
#include <fcntl.h>

#include <chrono>
#include <thread>

#include "../../headers/client_processor.hpp"

ClientProcessor::ClientProcessor(){}

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
    || !setupBuffers()
    || !setupSocket()){
        return false;
    }
    setupHeaderTypes();
    return true;
}

bool ClientProcessor::setupHashTables(){
    if(!username_to_key_.createTable(config::INITIAL_HASHTABLE_SIZE)){
        return false;
    }
    return true;
}

bool ClientProcessor::setupBuffers(){
    try{
        incoming_buffer_ = new uint8_t[config::READING_BUFFER_SIZE];
        outgoing_buffer_ = new uint8_t[config::READING_BUFFER_SIZE];
    } catch(const std::bad_alloc&){
        std::cout << "Error: insuficcient memory space!" << std::endl;
        return false;
    }
    return true;
}

void ClientProcessor::setupHeaderTypes(){
    ack_message_[header::HEAD_BITS_OFFSET] = UINT8_MAX;
    ack_message_[header::TYPE_OFFSET] = types::ACK;
    ack_message_[header::RECEIVER_OFFSET] = UINT8_MAX;
    ack_message_[header::RECEIVER_OFFSET + 1] = UINT8_MAX;
    ack_message_[header::RECEIVER_OFFSET + 2] = UINT8_MAX;
    ack_message_[header::RECEIVER_OFFSET + 3] = UINT8_MAX;
    ack_message_[header::PAYLOAD_LENGTH_OFFSET] = 0;
    ack_message_[header::PAYLOAD_LENGTH_OFFSET + 1] = 0;

    request_communication_[header::HEAD_BITS_OFFSET] = UINT8_MAX;
    request_communication_[header::TYPE_OFFSET] = types::SEND_REQUEST;
    request_communication_[header::RECEIVER_OFFSET] = UINT8_MAX;
    request_communication_[header::RECEIVER_OFFSET + 1] = UINT8_MAX;
    request_communication_[header::RECEIVER_OFFSET + 2] = UINT8_MAX;
    request_communication_[header::RECEIVER_OFFSET + 3] = UINT8_MAX;
    request_communication_[header::PAYLOAD_LENGTH_OFFSET] = 0;
    request_communication_[header::PAYLOAD_LENGTH_OFFSET + 1] = config::HOSTNAME_LENGTH;

    respond_communication_[header::HEAD_BITS_OFFSET] = UINT8_MAX;
    respond_communication_[header::TYPE_OFFSET] = types::REJECT_REQUEST;
    respond_communication_[header::RECEIVER_OFFSET] = UINT8_MAX;
    respond_communication_[header::RECEIVER_OFFSET + 1] = UINT8_MAX;
    respond_communication_[header::RECEIVER_OFFSET + 2] = UINT8_MAX;
    respond_communication_[header::RECEIVER_OFFSET + 3] = UINT8_MAX;
    respond_communication_[header::PAYLOAD_LENGTH_OFFSET] = 0;
    respond_communication_[header::PAYLOAD_LENGTH_OFFSET + 1] = config::HOSTNAME_LENGTH;

    auth_message_[header::HEAD_BITS_OFFSET] = UINT8_MAX;
    auth_message_[header::TYPE_OFFSET] = UINT8_MAX;
    auth_message_[header::RECEIVER_OFFSET] = UINT8_MAX;
    auth_message_[header::RECEIVER_OFFSET + 1] = UINT8_MAX;
    auth_message_[header::RECEIVER_OFFSET + 2] = UINT8_MAX;
    auth_message_[header::RECEIVER_OFFSET + 3] = UINT8_MAX;
    auth_message_[header::PAYLOAD_LENGTH_OFFSET] = 0;
    auth_message_[header::PAYLOAD_LENGTH_OFFSET + 1] = 0;
}

bool ClientProcessor::setupSocket(){
    int status;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *server_info;
    if((status = getaddrinfo(config::DEFAULT_IP, config::SERVER_PORT, &hints, &server_info)) != 0){
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
        bool receive_loop = true;
        while(receive_loop){
            Status rcvf_state = receiveFromServer();
            switch(rcvf_state){
                case Status::SUCCESS:{
                    Status check_state;
                    do{
                        check_state = messageProcessor();
                    }while (check_state == Status::SUCCESS);
                } break;
                case Status::NOTHING_TO_READ:{
                    receive_loop = false;
                } break;
                case Status::ERROR:{
                    return;
                } break;
                case Status::CLOSED_CONVERSATION:{
                    return;
                } break;
                case Status::INSUFFICIENT_BUFFER_SPACE:{
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
                default:{
                    std::cout << "Invalid return type!" << std::endl;
                    return;
                }
            }
        }
        if(send_register_){
            switch(sendMessage(auth_message_, credentials_length_ + config::HEADER_SIZE)){
                case Status::SUCCESS:{
                } break;
                case Status::RESOURCE_UNAVAILABLE:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                case Status::ERROR:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                default:{
                    std::cout << "Invalid return type!" << std::endl;
                    return;
                }
            }
            std::cout << "> " << std::flush;
            send_register_ = false;
        }
        if(send_request_){
            switch(sendMessage(request_communication_, config::HEADER_SIZE + config::HOSTNAME_LENGTH)){
                case Status::SUCCESS:{
                    std::cout << "Request sent correctly!" << std::endl;
                } break;
                case Status::RESOURCE_UNAVAILABLE:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                case Status::ERROR:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                default:{
                    std::cout << "Invalid return type!" << std::endl;
                    return;
                }
            }
            std::cout << "> " << std::flush;
            send_request_ = false;
        }
        if(respond_request_){
            switch(sendMessage(respond_communication_, config::HEADER_SIZE + config::HOSTNAME_LENGTH)){
                case Status::SUCCESS:{
                } break;
                case Status::RESOURCE_UNAVAILABLE:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                case Status::ERROR:{
                    std::cout << "Could not sent message!" << std::endl;
                } break;
                default:{
                    std::cout << "Invalid return type!" << std::endl;
                    return;
                }
            }
            std::cout << "> " << std::flush;
            respond_request_ = false;
        }
        if(send_message_){
            {
                std::unique_lock<std::mutex> lock_message(read_mutex_);
                switch(sendMessage(outgoing_buffer_, msg_len_)){
                    case Status::SUCCESS:{
                    } break;
                    case Status::RESOURCE_UNAVAILABLE:{
                        std::cout << "Could not sent message!" << std::endl ;
                    } break;
                    case Status::ERROR:{
                        std::cout << "Could not sent message!" << std::endl;
                    } break;
                    default:{
                        std::cout << "Invalid return type!" << std::endl;
                        return;
                    }
                }
                std::cout << "> " << std::flush;
                send_message_ = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config::LOOP_TIMER));
    }
}

/*
Sends message from a buffer.

SUCCESS - Message sent correctly.
RESOURCE_UNAVAILABLE - Could not send message.
ERROR - An unhandled error ocurred.
*/
Status ClientProcessor::sendMessage(uint8_t *buffer, int bytes_to_send){
    ssize_t total_bytes_sent = 0;
    ssize_t sent_bytes = 0;

    while(total_bytes_sent < bytes_to_send){
        if((sent_bytes = send(
            client_socket_,
            &buffer[total_bytes_sent],
            bytes_to_send - total_bytes_sent,
            0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return Status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of message failed.");
                return Status::ERROR;
            }
        }
        total_bytes_sent += sent_bytes;
    }
    return Status::SUCCESS;
}

/*
Copies data received from the server to a buffer until there is nothing else to read.

SUCCESS - All bytes were received and copied to the incming buffer.
CLOSED_CONVERSATION - The server closed the conversation.
INSUFFICIENT_BUFFER_SPACE - The server sent more bytes than the client can handle.
ERROR - An unhandled error ocurred.
*/
Status ClientProcessor::receiveFromServer(){
    ssize_t total_bytes_received = 0;
    ssize_t bytes_received = 0;
    if(byte_counter_ >= config::READING_BUFFER_SIZE){
        return Status::INSUFFICIENT_BUFFER_SPACE;
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
                return Status::ERROR;
            }
        } else if(bytes_received == 0){
            return Status::CLOSED_CONVERSATION;
            break;
        }
        total_bytes_received += bytes_received;
    }
    if(total_bytes_received == 0){
        return Status::NOTHING_TO_READ;
    }
    if(total_bytes_received > UINT32_MAX){
        return Status::ERROR;
    }
    byte_counter_ += static_cast<uint32_t>(total_bytes_received);
    writing_pointer_ = static_cast<uint8_t>((writing_pointer_ + total_bytes_received) % config::READING_BUFFER_SIZE);
    return Status::SUCCESS;
}

Status ClientProcessor::messageProcessor(){
    Status check_state = checkMessage();
    switch(check_state){
        case Status::SUCCESS:{
        } break;
        case Status::INCOMPLETE_MESSAGE:{
            return Status::INCOMPLETE_MESSAGE;
        } break;
        case Status::INVALID_MESSAGE:{
            std::cout << "Invalid message received!" << std::endl;
            resetIncomingBuffer();
            //report back to server
            return Status::SUCCESS;
        } break;
        default:{
            std::cout << "Invalid return type!" << std::endl;
            return Status::ERROR;
        }
    }

    Status act_state = actOnMessage();
    switch(act_state){
        case Status::SUCCESS:{
            cleanIncomingBuffer();
            return Status::SUCCESS;
        }
        case Status::INVALID_CLIENT:{
            std::cout << "Invalid message received!" << std::endl;
            cleanIncomingBuffer();
            return Status::SUCCESS;
        }
        case Status::INVALID_MESSAGE:{
            std::cout << "Invalid message received!" << std::endl;
            cleanIncomingBuffer();
            return Status::SUCCESS;
        } break;
        case Status::RESOURCE_UNAVAILABLE:{
            std::cout << "Resource unavailable error on messageProcessor!" << std::endl;
            cleanIncomingBuffer();
            return Status::RESOURCE_UNAVAILABLE;
        } break;
        case Status::ERROR:{
            return Status::ERROR;
        } break;
        default:{
            std::cout << "Invalid return type!" << std::endl;
            return Status::ERROR;
        }
    }
    return Status::ERROR;
}

/*
Checks if the entire message header + payload have been received.

SUCCESS - Message header is complete and valid.
INCOMPLETE_MESSAGE - The entire message has not been received.
INVALID_MESSAGE - The message does not follow the protocol rules.
*/
Status ClientProcessor::checkMessage(){
    if(!valid_header_){
        Status header_state = checkHeader();
        if(header_state != Status::SUCCESS){
            return header_state;
        }
    }
    if(byte_counter_ < static_cast<uint32_t>(payload_length_ + config::HEADER_SIZE)){
        return Status::INCOMPLETE_MESSAGE;
    }
    return Status::SUCCESS;
}

/*
Checks if the entire message header have been received.

SUCCESS - Message header is complete and valid.
INCOMPLETE_MESSAGE - The entire message has not been received.
INVALID_MESSAGE - The message does not follow the protocol rules.
*/
Status ClientProcessor::checkHeader(){
    if(byte_counter_ < config::HEADER_SIZE){
        return Status::INCOMPLETE_MESSAGE;
    }
    reading_pointer_ = starting_pointer_;
    // HEAD_BITS
    if((incoming_buffer_[reading_pointer_] ^ 0b11111111) != 0){
        return Status::INVALID_MESSAGE;
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
    return Status::SUCCESS;
}

/*
Performs different tasks depending on the type of message received.

SUCCESS - The message was valid and was handled correctly.
INVALID_CLIENT -
INVALID_MESSAGE -
RESOURCE_UNAVAILABLE - Unexpected error.
ERROR - An unhandled error ocurred.
*/
Status ClientProcessor::actOnMessage(){
    switch(type_){
        case types::USER:{
            if(payload_length_ == 0 || payload_length_ > config::MAX_MESSAGE_SIZE){
                return Status::INVALID_MESSAGE;
            }
            if(!printMessage()){
                return Status::ERROR;
            }
            for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
                ack_message_[i + 2] = static_cast<uint8_t>(sender_key_ >> ((config::CLIENT_KEY_LENGTH - i - 1) * 8));
            }
            Status ack_state = sendMessage(ack_message_, config::HEADER_SIZE);
            switch(ack_state){
                case Status::SUCCESS:{
                } break;
                case Status::RESOURCE_UNAVAILABLE:{
                    // should not return, rather be stored
                    return Status::RESOURCE_UNAVAILABLE;
                } break;
                case Status::ERROR:{
                    return Status::ERROR;
                } break;
                default:{
                    std::cout << "Invalid return type!" << std::endl;
                    return Status::ERROR;
                }
            }
        } break;
        case types::SEND_REQUEST:
        case types::REJECT_REQUEST:
        case types::ACCEPT_REQUEST:{
            if(payload_length_ != config::HOSTNAME_LENGTH){
                return Status::INVALID_MESSAGE;
            }
            std::string temp_username(config::HOSTNAME_LENGTH, 0);
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                temp_username[i] = incoming_buffer_[reading_pointer_];
                advanceReadingPointer();
            }
            if(!validateCredential(temp_username, config::HOSTNAME_LENGTH, config::HOSTNAME_LENGTH)){
                std::cout << "Invalid username received!" << std::endl;
                return Status::INVALID_CLIENT;
            }
            uint32_t temp_key = UINT32_MAX;
            if((temp_key = getUserKey(temp_username)) != UINT32_MAX){
                std::cout << "already known client" << std::endl;
                return Status::INVALID_CLIENT;
            } else{
                if(type_ == types::ACCEPT_REQUEST){
                    if(!addUser(sender_key_, temp_username)){
                        return Status::ERROR;
                    }
                } else if(type_ == types::SEND_REQUEST){
                    UsernameMapping usernameMapping;
                    usernameMapping.key = sender_key_;
                    for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                        usernameMapping.username[i] = temp_username[i];
                    }
                    incoming_requests_.push_back(usernameMapping);
                }
            }
        } break;
        case types::INFO:{
            if(payload_length_ != config::INFO_MESSAGE_LENGTH - config::HEADER_SIZE){
                return Status::INVALID_MESSAGE;
            }
            uint8_t info_type = incoming_buffer_[reading_pointer_];
            advanceReadingPointer();
            switch(info_type){
                case info::VALID_REGISTER:{
                    std::cout << "Info: Logged in!" << std::endl;
                    logged_in_ = true;
                } break;
                case info::INVALID_CREDENTIAL:{
                    std::cout << "Info: Invalid credentials. Please try again!" << std::endl;
                } break;
                case info::NOT_UNIQUE:{
                    std::cout << "Info: " << "\"" << username_ << "\" is not available!" << std::endl;
                } break;
                case info::ALREADY_LOGGED_IN:{
                    std::cout << "Info: Already logged in!" << std::endl;
                } break;
                case info::INVALID_MESSAGE:{
                    std::cout << "Info: Message sent, was invalid!" << std::endl;
                } break;
                case info::INVALID_CLIENT:{
                    std::cout << "Info: The targeted receiver was invalid!" << std::endl;
                } break;
                case info::ALREADY_SENT_REQUEST:{
                    std::cout << "Info: Already sent request to that user!" << std::endl;
                } break;
                case info::ALREADY_KNOWN_CLIENT:{
                    std::cout << "Info: You already know this client!" << std::endl;
                    // add them back if lost other user credential
                } break;
                case info::REQUEST_ALREADY_RECEIVED:{
                    std::cout << "Info: This client sent you a request!" << std::endl;
                } break;
                case info::UNAUTHENTICATED_USER:{
                    std::cout << "Info: You must authenticate first!" << std::endl;
                } break;
                case info::SEND_ERROR:{
                    std::cout << "Info: message could not be delivered!" << std::endl;
                } break;
                case info::COULD_NOT_REGISTER:{
                    std::cout << "Info: Server cannot register more users :(" << std::endl;
                    return Status::ERROR;
                } break;
            }
            std::cout << "> " << std::flush;
        } break;
        case types::ACK:{
            if(sender_key_ != UINT32_MAX){
                char *user = getUserFromKey(sender_key_);
                if(user == nullptr){
                    return Status::INVALID_CLIENT;
                }
                std::cout << "Message to " << user << " has been delivered!" << std::endl;
            }
        } break;
        default:{
            return Status::INVALID_MESSAGE;
        }
    }
    return Status::SUCCESS;
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

// Resets incoming buffer after an invalid message has been received.
void ClientProcessor::resetIncomingBuffer(){
    valid_header_ = false;
    starting_pointer_ = 0;
    writing_pointer_ = 0;
    reading_pointer_ = 0;
    byte_counter_ = 0;
    payload_length_ = UINT16_MAX;
    type_ =types::INVALID_TYPE;
    sender_key_ = UINT32_MAX;
}

// Resets incoming buffer indicators to process new messages.
void ClientProcessor::cleanIncomingBuffer(){
    valid_header_ = false;
    starting_pointer_ = reading_pointer_;
    byte_counter_ -= (payload_length_ + config::HEADER_SIZE);
    payload_length_ = UINT16_MAX;
    type_ =types::INVALID_TYPE;
    sender_key_ = UINT32_MAX;
}

// Central loop handler that contains the authentication loop and central menu loop.
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
            << "========================================" << std::endl
            << "Welcome Menu." << std::endl
            << "========================================" << std::endl
            << "1) Login. (implement later)" << std::endl
            << "2) Register." << std::endl
            << "3) Enter Main Menu." << std::endl
            << "4) Exit." << std::endl
            << "> ";
        ans = userNumericInput();
        switch(ans){
            case 1:{
            } break;
            case 2:{
                std::string tmp_username;
                std::cout << "Choose a username. It must only contain letters, numbers and underscores (_)."
                << std::endl << "The maximum size is "
                << static_cast<uint>(config::HOSTNAME_LENGTH) << " characters."
                << std::endl << "Username: ";
                while(tmp_username.length() == 0){
                    std::getline(std::cin, tmp_username);
                }
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
                while(tmp_password.length() == 0){
                    std::getline(std::cin, tmp_password);
                }
                if(!validateCredential(tmp_password, config::MIN_PASSWORD_LENGTH, config::MAX_PASSWORD_LENGTH)){
                    break;
                }

                std::cout << "Adequate credentials!" << std::endl;

                username_ = tmp_username;
                password_ = tmp_password;
                credentials_length_ = config::HOSTNAME_LENGTH + static_cast<uint8_t>(password_.length());
                uint32_t username_length = static_cast<uint32_t>(username_.length());
                uint32_t password_length = static_cast<uint32_t>(password_.length());

                auth_message_[1] = types::REGISTER;
                auth_message_[7] = credentials_length_;
                for(uint32_t i = 0; i < username_length; i++){
                    auth_message_[i + config::HEADER_SIZE] = username_[i];
                }
                for(uint32_t i = username_length; i < config::HOSTNAME_LENGTH; i++){
                    auth_message_[i + config::HEADER_SIZE] = 0;
                }
                for(uint32_t i = 0; i < password_length; i++){
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

// Message loop to handle user messaging.
bool ClientProcessor::messageInputLoop(){
    int main_ans = -1;
    while(program_running_){
        if(main_ans == 2 || main_ans == 3 || main_ans == 4 || main_ans == 5){
            std::cout << "> ";
        } else{
            std::cout
                << "========================================" << std::endl
                << "Main Menu." << std::endl
                << "User     : " << username_ << std::endl
                << "Receiver : " << receiving_username_ << std::endl;
            if(message_.length() > 20){
                std::cout << "Draft    : " << message_.substr(0, 20) << "..." << std::endl;
            } else{
                std::cout << "Draft    : " << message_ << std::endl;
            }
                std::cout << "========================================" << std::endl
                << "1) Set message" << std::endl
                << "2) Choose recipient" << std::endl
                << "3) Send message" << std::endl
                << "4) Send request" << std::endl
                << "5) Requests (" << static_cast<std::size_t>(incoming_requests_.size()) << ")" << std::endl
                << "6) Reload" << std::endl
                << "7) Exit" << std::endl
                << "> ";
        }
        main_ans = userNumericInput();
        switch(main_ans){
            case 1:{
            Status result = setMessage();
            switch(result){
                case Status::SUCCESS:{
                    std::cout << "Message set!" << std::endl;
                } break;
                case Status::INVALID_MESSAGE:{
                    std::cout << "Invalid message, please try again!" << std::endl;
                } break;
                default:{
                    std::cout << "Invalid return type!" << std::endl;
                    return false;
                }
            }
            } break;
            case 2:{
                Status result = setReceiver();
                switch(result){
                    case Status::SUCCESS:{
                    } break;
                    case Status::NOTHING_TO_DO:{
                    } break;
                    case Status::INVALID_MESSAGE:{
                        std::cout << "Invalid client, please try again!" << std::endl;
                    } break;
                    case Status::PROGRAMMING_ERROR:{
                        std::cout << "There's a code error on setReceiver!" << std::endl;
                        return false;
                    }
                    default:{
                        std::cout << "Invalid return type!" << std::endl;
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
                    uint32_t username_length = static_cast<uint32_t>(temp_username.length());
                    for(uint32_t i = 0; i < username_length; i++){
                        request_communication_[i + config::HEADER_SIZE] = temp_username[i];
                    }
                    for(uint32_t i = username_length; i < config::HOSTNAME_LENGTH; i++){
                        request_communication_[i + config::HEADER_SIZE] = 0;
                    }
                    send_request_ = true;
                } else{
                    std::cout << "Invalid username!" << std::endl;
                }
            } break;
            case 5:{
                if(incoming_requests_.empty()){
                    std::cout << "No requests available!" << std::endl;
                    break;
                }
                std::cout << "Input a number corresponding to a request to decide what to do with it. Input 0 to exit." << std::endl;
                std::cout << "Requests: " << std::endl;

                int ctr = 1;
                for(auto it = incoming_requests_.begin(); it != incoming_requests_.end(); it++){
                    std::cout << ctr++ << ": " << it->username << std::endl;
                }

                int ans = userNumericInput();
                if(ans == 0){
                    break;
                }
                if(ans > ctr){
                    std::cout << "Invalid request selected!" << std::endl;
                     break;
                }

                auto requester_user = incoming_requests_.begin();
                for(int i = 0; i < ans - 1; i++){
                    requester_user++;
                }
                if(requester_user == incoming_requests_.end()){
                    break;
                }
                std::cout << "Select one option." << std::endl
                << "Request from: " << requester_user->username << std::endl
                << "1. Accept" << std::endl
                << "2. Reject" << std::endl
                << "3. Exit" << std::endl
                << "> ";
                ans = userNumericInput();
                if(ans == 1 || ans == 2){
                    char *ref_username = requester_user->username;
                    UsernameMapping usernameMapping;
                    usernameMapping.key = requester_user->key;
                    for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                        usernameMapping.username[i] = ref_username[i];
                    }
                    if(ans == 1){
                        if(username_to_key_.searchNode(stringHash(requester_user->username))){
                            std::cout << "Info: You already know this client!" << std::endl;
                            break;
                        }
                        if(!username_to_key_.insertNode(stringHash(requester_user->username), usernameMapping)){
                            return false;
                        }
                        respond_communication_[header::TYPE_OFFSET] = types::ACCEPT_REQUEST;
                    } else{
                        respond_communication_[header::TYPE_OFFSET] = types::REJECT_REQUEST;
                    }
                    incoming_requests_.erase(requester_user);
                    for(int i = 0; i < 4; i++){
                        respond_communication_[header::RECEIVER_OFFSET + i] = static_cast<uint8_t>(usernameMapping.key << ((3 - i) * 8));
                    }
                    respond_request_ = true;
                }
            } break;
            case 6:{
            } break;
            case 7:{
                program_running_ = false;
                return true;
            } break;
        }
    }
    return false;
}

/*
Sets message to send in a buffer.

SUCCESS - Message was set correctly.
INVALID_MESSAGE - Message was not set.
*/
Status ClientProcessor::setMessage(){
    std::cout << "Message: ";
    std::getline(std::cin, message_);
    if(message_.length() == 0 || message_.length() > config::MAX_MESSAGE_SIZE){
        message_.clear();
        return Status::INVALID_MESSAGE;
    }

    {
        std::unique_lock<std::mutex> lock_message(read_mutex_);
        uint16_t message_length = static_cast<uint16_t>(message_.length());

        outgoing_buffer_[0] = 255;
        outgoing_buffer_[1] = types::USER;
        outgoing_buffer_[6] = static_cast<uint8_t>(message_length >> 8);
        outgoing_buffer_[7] = static_cast<uint8_t>(message_length);

        for(int i = 0; i < message_length; i++){
            outgoing_buffer_[header::PAYLOAD_OFFSET + i] = message_[i];
        }
        msg_len_ = 8 + message_length;
    }
    return Status::SUCCESS;
}

/*
Sets destinatory from a list of known users.

SUCCESS - Receiver was set correctly.
NOTHING_TO_DO - No known users or none selected.
INVALID_CLIENT - Receiver was not set.
PROGRAMMING_ERROR - There's an issue with how the receiver is set.
*/
Status ClientProcessor::setReceiver(){
    if(username_to_key_.getDataCount() == 0){
        std::cout << "No known users, request a user to establish a connection first!" << std::endl;
        return Status::NOTHING_TO_DO;
    }
    std::cout << "Input a number corresponding to a user to select a recipient. Input 0 to exit." << std::endl;
    std::cout << "Known users: " << std::endl;
    int ctr = 1;
    username_to_key_.resetListPtr();
    do{
        auto *list = username_to_key_.getListPtr();
        for(auto it = list->begin(); it != list->end(); it++){
            std::cout  << ctr++ << ": " << it->data_.username << std::endl;
        }
    } while (username_to_key_.advanceListPtr());

    std::cout << "> ";

    int ans = userNumericInput();
    if(ans == 0){
        return Status::NOTHING_TO_DO;
    }

    if(ans > ctr){
        std::cout << "Invalid request selected!" << std::endl;
            return Status::INVALID_CLIENT;
    }
    ctr = 1;
    UsernameMapping receiver_data;
    receiver_data.key = UINT32_MAX;
    username_to_key_.resetListPtr();
    do{
        auto *list = username_to_key_.getListPtr();
        for(auto it = list->begin(); it != list->end(); it++){
            ctr++;
            if(ctr > ans){
                receiver_data = it->data_;
                break;
            }
        }
        if(ctr > ans){
            break;
        }
    } while(username_to_key_.advanceListPtr());

    if(receiver_data.key == UINT32_MAX){
        return Status::PROGRAMMING_ERROR;
    }

    receiving_username_ = receiver_data.username;
    receiver_key_ = receiver_data.key;

    outgoing_buffer_[header::RECEIVER_OFFSET] = static_cast<uint8_t>(receiver_key_ >> 24);
    outgoing_buffer_[header::RECEIVER_OFFSET + 1] = static_cast<uint8_t>(receiver_key_ >> 16);
    outgoing_buffer_[header::RECEIVER_OFFSET + 2] = static_cast<uint8_t>(receiver_key_ >> 8);
    outgoing_buffer_[header::RECEIVER_OFFSET + 3] = static_cast<uint8_t>(receiver_key_);
    return Status::SUCCESS;
}

// Validates if a credential contains valid characters and is of allowed size.
bool ClientProcessor::validateCredential(const std::string &credential, uint8_t min_length, uint8_t max_length){
    if(credential.size() < min_length || credential.size() > max_length){
        std::cout << "Credential is too long or too short!"  << std::endl;
        return false;
    }
    bool valid_credential = true;
    uint32_t credential_length = static_cast<uint32_t>(credential.length());
    for(uint32_t i = 0; i < credential_length; i++){
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
    return valid_credential;
}

// Add a user to the "list" of known users.
bool ClientProcessor::addUser(uint32_t key, const std::string &username){
    UsernameMapping user;
    user.key = key;
    std::memcpy(user.username, username.data(), config::HOSTNAME_LENGTH);
    int hash_key = stringHash(user.username);
    if(!username_to_key_.insertNode(hash_key, user)){
        return false;
    }
    return true;
}

// Gets a key corresponding to a specific user. Returns UINT32_MAX if the user does not exist.
uint32_t ClientProcessor::getUserKey(const std::string &temp_username){
    char username [config::HOSTNAME_LENGTH + 1] = {0};
    if(temp_username.size() > config::HOSTNAME_LENGTH + 1){
        return UINT32_MAX;
    }
    std::strncpy(username, temp_username.c_str(), config::HOSTNAME_LENGTH);
    int hash_key = stringHash(username);
    if(!username_to_key_.searchNode(hash_key)){
        return UINT32_MAX;
    }

    auto list = username_to_key_.getList(hash_key);
    if(!list){
        return UINT32_MAX;
    }
    for(auto it = list->begin(); it != list->end(); it++){
        bool equal_usernames = true;
        for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
            if(it->data_.username[i] != temp_username[i]){
                equal_usernames = false;
                break;
            }
        }
        if(equal_usernames){
            return it->data_.key;
        }
    }

    return UINT32_MAX;
}

// Gets a user's username from their key. Returns nullptr if the user does not exist.
char* ClientProcessor::getUserFromKey(uint32_t key){
    username_to_key_.resetListPtr();
    do{
        auto *list = username_to_key_.getListPtr();
        for(auto it = list->begin(); it != list->end(); it++){
            if(it->data_.key == key){
                return it->data_.username;
            }
        }
    } while (username_to_key_.advanceListPtr());
    return nullptr;
}

int ClientProcessor::userNumericInput(){
    std::string ans;
    int parsed_ans = -1;
    while(parsed_ans == -1){
        while(ans.length() == 0){
            std::getline(std::cin, ans);
        }
        if(ans.length() < config::MAX_LENGTH_OF_INT_CHOICE){
            if(integerCheck(ans, static_cast<uint32_t>(ans.length()))){
                parsed_ans = std::stoi(ans);
            } else{
                std::cout << "Invalid input, please enter a number." << std::endl;
                ans.clear();
            }
        } else{
        std::cout << "Invalid input, please enter a number from the selection." << std::endl;
        ans.clear();
        }
    }
    return parsed_ans;
}

bool ClientProcessor::integerCheck(const std::string &string, uint32_t length){
    for(uint32_t i = 0; i < length; i++){
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

uint32_t ClientProcessor::stringHash(const char *str){
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)){
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}