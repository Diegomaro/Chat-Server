#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <cstring>
#include <iostream>
#include <cerrno>

#include <fcntl.h>

#include <cstdint>

#include <chrono>
#include <thread>

#include "../../headers/server.hpp"

Server::Server(){
    memset(&processed_ack_message_, 0, sizeof(processed_ack_message_));
    memset(&delivered_ack_message_, 0, sizeof(delivered_ack_message_));
    memset(&request_communication_message_, 0, sizeof(request_communication_message_));
    memset(&accept_communication_message_, 0, sizeof(accept_communication_message_));
    memset(&authentication_message_, 0, sizeof(authentication_message_));
}

Server::~Server(){
    if(listener_fd_!= -1){
        close(listener_fd_);
    }
    if(epoll_fd_ != -1){
        close(epoll_fd_);
    }

    clients_.resetNodeIndex();
    while(clients_.hasNodes()){
        if(clients_.hasNode()){
            int socket = (clients_.getNode()->key_);
            if(socket != -1){
                std::cout << "closing socket " << socket << std::endl;
                close(socket);
            }
        }
        clients_.advanceNode();
    }
    if(buffer_pool_){
        delete [] buffer_pool_;
        buffer_pool_ = nullptr;
    }
    if(receiver_buffer_){
        delete [] receiver_buffer_;
        receiver_buffer_ = nullptr;
    }
    if(sending_buffer_){
        delete [] sending_buffer_;
        sending_buffer_ = nullptr;
    }
}

bool Server::setupServer(){
    if(!setupHashTables()
    || !setupBuffer()
    || !setupHeaderTypes()
    || !setupListenerSocket()){
        return false;
    }
    return true;
}

void Server::centralLoop(){
    while(true){
        int ready_polls = 0;
        if((ready_polls = epoll_wait(epoll_fd_, events_, config::MAX_EVENTS, -1)) == -1){
            perror("epoll wait failed");
            return;
        }
        for (int i = 0; i < ready_polls; i++){
            if(events_[i].data.fd == listener_fd_){
                bool accept_loop = true;
                while(accept_loop){
                    Status accept_state = acceptConnection();
                    switch(accept_state){
                        case Status::SUCCESS:{
                        } break;
                        case Status::NOTHING_TO_DO:{
                            accept_loop = false;
                        } break;
                        case Status::ERROR:{
                            //
                            return;
                        } break;
                        case Status::EXCEEDED_CLIENT_MAX:{
                            //
                            return;
                        } break;
                    }
                }
            } else if (events_[i].events & EPOLLIN){
                int sender_socket = events_[i].data.fd;
                bool receive_loop = true;
                while(receive_loop){
                    Status rcvf_state = receiveFromClient(sender_socket);
                    switch(rcvf_state){
                        case Status::SUCCESS:{
                            Status check_state = checkMessage(sender_socket);
                            //work here
                            switch(check_state){
                                case Status::SUCCESS:{
                                    Status act_state = actOnMessage(sender_socket);
                                    switch(act_state){
                                        //
                                    }
                                    if(!cleanClientBuffer(sender_socket)){
                                       return;
                                    }
                                } break;
                                case Status::ERROR:{
                                    return;
                                } break;
                                case Status::INVALID_MESSAGE:{
                                    //send signal of error to user
                                    receive_loop = false;
                                } break;
                                case Status::INVALID_CLIENT:{
                                    //send signal of error to user
                                    receive_loop = false;
                                } break;
                            }
                           //if missing timeout
                        } break;
                        case Status::NOTHING_TO_READ:{
                            receive_loop = false;
                        } break;
                        case Status::INVALID_CLIENT:{
                            return;
                        } break;
                        case Status::CLOSED_CONVERSATION:{
                            if(!closeConnection(sender_socket)){
                                return;
                            }
                            receive_loop = false;
                            return; // to test for memory leaks
                        } break;
                        case Status::ERROR:{
                            return;
                        } break;
                        case Status::EXCEEDED_CLIENT_BUFFER_SIZE:{
                            // return error message to client and restart buffer segments
                        } break;
                    }
                }
            }
        }
        if(ready_polls == 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool Server::setupHashTables(){
    if(!clients_.createTable(config::INITIAL_HASHTABLE_SIZE)){
        return false;
    }
    if(!client_key_to_socket_.createTable(config::INITIAL_HASHTABLE_SIZE)){
        return false;
    }
    if(!username_to_client_key_.createTable(config::INITIAL_HASHTABLE_SIZE)){
        return false;
    }
    if(!client_key_to_known_keys_.createTable(config::INITIAL_HASHTABLE_SIZE)){
        return false;
    }
    return true;
}

bool Server::setupBuffer(){
    if(buffer_pool_ || receiver_buffer_ || sending_buffer_){
        return false;
    }
    buffer_pool_ = new uint8_t [config::BUFFER_SIZE];
    if(!buffer_pool_){
        return false;
    }
    receiver_buffer_ = new uint8_t [config::BUFFER_READING_SIZE];
    if(!receiver_buffer_){
        return false;
    }
    sending_buffer_ = new uint8_t [config::READING_BUFFER_SIZE];
    if(!sending_buffer_){
        return false;
    }
    uint32_t current_address = 0;
    for(int i = 0; i < config::TOTAL_BUFFER_SEGMENTS; i++){
        available_buffers_.insertTail(current_address);
        current_address += config::BUFFER_SEGMENT_SIZE;
    }
    return true;
}

bool Server::setupHeaderTypes(){
    if(!processed_ack_message_ || !delivered_ack_message_ || !authentication_message_ || !request_communication_message_){
        return false;
    }
    processed_ack_message_[0] = UINT8_MAX;
    processed_ack_message_[1] = types::ACK;
    processed_ack_message_[2] = UINT8_MAX;
    processed_ack_message_[3] = UINT8_MAX;
    processed_ack_message_[4] = UINT8_MAX;
    processed_ack_message_[5] = UINT8_MAX;
    processed_ack_message_[6] = 0;
    processed_ack_message_[7] = 0;

    delivered_ack_message_[0] = UINT8_MAX;
    delivered_ack_message_[1] = types::ACK;
    delivered_ack_message_[2] = UINT8_MAX;
    delivered_ack_message_[3] = UINT8_MAX;
    delivered_ack_message_[4] = UINT8_MAX;
    delivered_ack_message_[5] = UINT8_MAX;
    delivered_ack_message_[6] = 0;
    delivered_ack_message_[7] = 0;

    authentication_message_[0] = UINT8_MAX;
    authentication_message_[1] = types::REGISTER;
    authentication_message_[2] = UINT8_MAX;
    authentication_message_[3] = UINT8_MAX;
    authentication_message_[4] = UINT8_MAX;
    authentication_message_[5] = UINT8_MAX;
    authentication_message_[6] = 0;
    authentication_message_[7] = config::AUTH_PAYLOAD_LENGTH;
    authentication_message_[8] = 0;

    request_communication_message_[0] = UINT8_MAX;
    request_communication_message_[1] = types::SEND_REQUEST;
    request_communication_message_[2] = UINT8_MAX;
    request_communication_message_[3] = UINT8_MAX;
    request_communication_message_[4] = UINT8_MAX;
    request_communication_message_[5] = UINT8_MAX;
    request_communication_message_[6] = 0;
    request_communication_message_[7] = config::HOSTNAME_LENGTH;
    return true;
}

bool Server::setupListenerSocket(){
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = 0;
    if((status = getaddrinfo(NULL, config::SERVER_PORT, &hints, &res)) != 0){
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        return false;
    }
    if((listener_fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1){
        perror("socket failed");
        return false;
    }
    int yes = 1;
    if (setsockopt(listener_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
        perror("setsockopt SO_REUSEADDR failed");
    }
    if((bind(listener_fd_, res->ai_addr, res->ai_addrlen)) == -1){
        perror("bind failed");
        return false;
    }
    if(fcntl(listener_fd_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return false;
    }
    freeaddrinfo(res);
    if ((epoll_fd_ = epoll_create1(0)) == -1){
        perror("epoll failed");
        return false;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listener_fd_;
    if(listen(listener_fd_, config::BACKLOG) == -1){
        perror("listen failed");
        return false;
    }
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listener_fd_, &ev);
    return true;
}

/*
Accepts a incoming connection request and adds them as as a client.
Returns EXCEEDED_CLIENT_MAX, NOTHING_TO_DO, ERROR, SUCCESS.
*/
Status Server::acceptConnection(){
    if(clients_.getDataCount() + 1 >= config::MAX_HOSTS){
        return Status::EXCEEDED_CLIENT_MAX;
    }
    sockaddr_storage client_sockaddr;
    socklen_t client_sockaddr_len = sizeof(client_sockaddr);

    if((pending_client_fd_ = accept(listener_fd_, (struct sockaddr *)&client_sockaddr, &client_sockaddr_len)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return Status::NOTHING_TO_DO;
        } else{
            perror("accept failed");
            return Status::ERROR;
        }
    }
    if(!addClient(client_sockaddr)){
        return Status::EXCEEDED_CLIENT_MAX;
    }
    if(fcntl(pending_client_fd_, F_SETFL, O_NONBLOCK) == -1){
        perror("non blocking failed");
        return Status::ERROR;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = pending_client_fd_;
    if(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pending_client_fd_, &ev) == -1){
        perror("epoll failed");
        return Status::ERROR;
    }
    return Status::SUCCESS;
}

// Registers a client with their socket info. Gives each registered client a buffer segment to occupy.
bool Server::addClient(const sockaddr_storage& client_sockaddr){
    Client new_client;
    new_client.name[0] = '\0';
    void* addr;
    if(client_sockaddr.ss_family == AF_INET){
        sockaddr_in* ipv4 = (sockaddr_in*)&client_sockaddr;
        addr = &(ipv4->sin_addr);
        new_client.port = ntohs(ipv4->sin_port);
    }
    else {
        sockaddr_in6* ipv6 = (sockaddr_in6*)&client_sockaddr;
        addr = &(ipv6->sin6_addr);
        new_client.port = ntohs(ipv6->sin6_port);
    }
    inet_ntop(client_sockaddr.ss_family, addr, new_client.ip, sizeof(new_client.ip));
    if(available_buffers_.isEmpty()){
        return false;
    }
    new_client.buffer_pointers[0] = available_buffers_.getHead();
    available_buffers_.deleteHead();
    new_client.buffer_pointers_count = 1;
    new_client.starting_pointer = new_client.buffer_pointers[0];
    new_client.reading_pointer = new_client.buffer_pointers[0];
    new_client.writing_pointer = new_client.buffer_pointers[0];

    if(!clients_.insertNode(pending_client_fd_, new_client)){
        return false;
    }
    return true;
}

// Closes a client connection. Returns occupied buffer segments to the buffer pool.
bool Server::closeConnection(int client_socket){
    if(close(client_socket) == -1){
        perror("clossing failed");
        return false;
    }
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return false;
    }
    int8_t buffers_erased = 0;
    for(int i = 0; i < config::BUFFER_SEGMENTS_PER_CLIENT; i++){
        if(buffers_erased >= client->buffer_pointers_count){
            break;
        }
        if(client->buffer_pointers[i] != UINT32_MAX){
            available_buffers_.insertHead(client->buffer_pointers[i]);
            buffers_erased++;
        }
    }
    if(!client_key_to_socket_.deleteNode(client->sender_key)){
        return false;
    }
    if(!username_to_client_key_.deleteNode(stringHash(client->name))){
        return false;
    }
    if(!clients_.deleteNode(client_socket)){
        return false;
    }
    std::cout << "manual close of socket "  << client_socket << std::endl;
    return true;
}

/*
Copies an incoming message (possibly fragmented) to the corresponding client buffers.
Returns INVALID_CLIENT, ERROR, NOTHING_TO_READ, CLOSED_CONVERSATION, EXCEEDED_CLIENT_BUFFER_SIZE, INSUFFICIENT_BUFFER_SPACE, SUCCESS.
*/
Status Server::receiveFromClient(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::ERROR;
    }
    receiver_buffer_[0] = '\0';
    int bytes_received = 0;
    if((bytes_received = recv(client_socket, receiver_buffer_, config::BUFFER_READING_SIZE, 0)) == -1){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            return Status::NOTHING_TO_READ;
        } else{
            perror("An error ocurred while receiving from client.");
            return Status::ERROR;
        }
    }
    if(bytes_received == 0){
        return Status::CLOSED_CONVERSATION;
    }

    int bytes_remaining = bytes_received;
    int msg_buffer_offset = 0;
    while(bytes_remaining > 0){
        uint32_t available_segment_bytes = client->getRemainingBytesWriting();
        if(available_segment_bytes > bytes_remaining){
            memcpy(&buffer_pool_[client->writing_pointer], &receiver_buffer_[msg_buffer_offset], bytes_remaining);
            client->writing_pointer += bytes_remaining;
            client->byte_counter += bytes_remaining;
            bytes_remaining = 0;
        }else {
            memcpy(&buffer_pool_[client->writing_pointer], &receiver_buffer_[msg_buffer_offset], available_segment_bytes);
            client->byte_counter += available_segment_bytes;
            msg_buffer_offset += available_segment_bytes;
            bytes_remaining -= available_segment_bytes;
            if(client->buffer_pointers_count + 1 >= config::BUFFER_SEGMENTS_PER_CLIENT){
                if(checkMessage(client_socket) == Status::SUCCESS){
                    std::cout << "work on it later" << std::endl;
                    // delete old message
                }
                return Status::EXCEEDED_CLIENT_BUFFER_SIZE;
            } else if(available_buffers_.isEmpty()){
                return Status::INSUFFICIENT_BUFFER_SPACE;
            } else{
                uint32_t new_buffer_segment = available_buffers_.getHead();
                client->buffer_pointers[(client->writing_buffer + 1) % 128] = new_buffer_segment;
                available_buffers_.deleteHead();
                client->buffer_pointers_count++;
                client->writing_buffer = (client->writing_buffer + 1) % 128;
                client->writing_pointer = new_buffer_segment;
            }
        }
    }
    return Status::SUCCESS;
}

/*
Checks if the entire message header + payload have been received.
returns INVALID_CLIENT, ERROR, INVALID_MESSAGE, INCOMPLETE_MESSAGE, SUCCESS.
*/
Status Server::checkMessage(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::ERROR;
    }
    if(!client->valid_header_){
        Status header_state = checkHeader(client_socket);
        if(header_state != Status::SUCCESS){
            return header_state;
        }
    }

    if(client->byte_counter < client->payload_length + config::HEADER_SIZE){
        return Status::INCOMPLETE_MESSAGE;
    }
    return Status::SUCCESS;
}

/*
Verifies that a message has a valid header and replaces target key with sender key.
returns INVALID_CLIENT, ERROR, INVALID_MESSAGE, INCOMPLETE_MESSAGE, SUCCESS.
*/
Status Server::checkHeader(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::ERROR;
    }
    client->reading_pointer = client->starting_pointer;
    if(client->byte_counter < config::HEADER_SIZE){
        return Status::INCOMPLETE_MESSAGE;
    }
    // HEAD_BITS
    if((buffer_pool_[client->reading_pointer] ^ 0xFF) != 0){
        return Status::INVALID_MESSAGE;
    }
    client->advanceReadingPointer();

    // TYPE
    if(client->type == types::INVALID_TYPE){
        client->type = buffer_pool_[client->reading_pointer];
    }
    client->advanceReadingPointer();

    // HOST_KEY
    if(client->receiver_key == UINT32_MAX){
        uint32_t tmp_pointer = client->reading_pointer;
        uint8_t tmp_buffer = client->reading_buffer;

        client->receiver_key = 0;

        for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
            client->receiver_key += (buffer_pool_[client->reading_pointer]) << ((config::CLIENT_KEY_LENGTH - 1 - i) * 8);
            client->advanceReadingPointer();
        }
        if(client->receiver_key != UINT32_MAX){

            if(!client_key_to_socket_.searchNode(client->receiver_key)){
                return Status::INVALID_CLIENT;
            }
            client->receiver_fd = *client_key_to_socket_.getNode(client->receiver_key);

            client->reading_pointer = tmp_pointer;
            client->reading_buffer = tmp_buffer;
            for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
                buffer_pool_[client->reading_pointer] = client->sender_key << ((config::CLIENT_KEY_LENGTH - 1 - i) * 8);
                client->advanceReadingPointer();
            }
        }
    } else{
        for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
            client->advanceReadingPointer();
        }
    }

    // PAYLOAD_LENGTH
    if(client->payload_length == UINT16_MAX){
        client->payload_length = 0;
        client->payload_length = buffer_pool_[client->reading_pointer] << 8;
        client->advanceReadingPointer();
        client->payload_length = client->payload_length | (buffer_pool_[client->reading_pointer]);
    } else{
        client->advanceReadingPointer();
    }
    client->advanceReadingPointer();
    client->valid_header_ = true;
    return Status::SUCCESS;
}

/*
Performs different tasks depending on the type of message received.
returns ERROR, INVALID_MESSAGE, UNAUTHENTICATED_USER, RESOURCE_UNAVAILABLE, INVALID_CLIENT, INCOMPLETE_MESSAGE, SUCCESS.
*/
Status Server::actOnMessage(int client_socket){
    Client* client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::INVALID_CLIENT;
    }
    switch(client->type){
        case types::USER:{
            if(!client->logged_in){
                return Status::UNAUTHENTICATED_USER;
            }
            if(client->payload_length == 0 || client->payload_length > config::MAX_MESSAGE_SIZE){
                return Status::INVALID_MESSAGE;
            }

            if(client_key_to_known_keys_.searchNode(client->sender_key)){
                LinkedList<uint32_t> *known_users = *client_key_to_known_keys_.getNode(client->sender_key);
                if(!known_users->searchNode(client->receiver_key)){
                    return Status::INVALID_CLIENT;
                }
            } else{
                return Status::ERROR;
            }

            if(!clients_.searchNode(client->receiver_fd)){
                return Status::INVALID_CLIENT;
                // later it should be changed to store all client keys, regardless of whether online or not.
                // If client is not available it should be stored in some file. (much later)
            }
            Status ack_state = sendStatusMessage(
                client_socket,
                client_socket,
                processed_ack_message_,
                config::HEADER_SIZE
            );
            Status send_state = sendToClient(client_socket);

            switch(ack_state){
                case Status::RESOURCE_UNAVAILABLE:{
                    // should not return, rather be stored
                    return Status::RESOURCE_UNAVAILABLE;
                } break;
                case Status::ERROR:{
                    return Status::ERROR;
                } break;
            }
            /*
            verify if target is available. If not available, store the message in txt file.
            Otherwise request sending buffer.
            */
            switch(send_state){
                case Status::RESOURCE_UNAVAILABLE:{
                    //should not return, rather be stored
                    return Status::RESOURCE_UNAVAILABLE;
                } break;
                case Status::ERROR:{
                    return Status::ERROR;
                } break;
            }
            return Status::SUCCESS;
        } break;
        case types::REGISTER:{
            if(client->logged_in){
                // do smth
            }

            if(client->payload_length < config::HOSTNAME_LENGTH + config::MIN_PASSWORD_LENGTH
            || client->payload_length > config::HOSTNAME_LENGTH + config::MAX_PASSWORD_LENGTH){
                return Status::INVALID_MESSAGE;
            }

            //CHECK CREDENTIALS
            uint8_t username [config::HOSTNAME_LENGTH];
            uint32_t usr_ctr = 0;
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                username[i] = buffer_pool_[client->reading_pointer];
                if((username[i] > 0 && username[i] < 48)
                || (username[i] > 57 && username[i] < 65)
                || (username[i] > 90 && username[i] < 95)
                || (username[i] > 95 && username[i] < 97)
                || username[i] > 122){
                    authentication_message_[8] = auth::INVALID_CREDENTIAL;
                    return sendStatusMessage(
                        client_socket,
                        client_socket,
                        authentication_message_,
                        config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH
                    );
                }
                usr_ctr += username[i] != 0 ? 1 : 0;
                client->advanceReadingPointer();
            }
            if(usr_ctr < 1){
                authentication_message_[8] = auth::INVALID_CREDENTIAL;
                return sendStatusMessage(
                    client_socket,
                    client_socket,
                    authentication_message_,
                    config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH
                );
            }
            uint8_t password [client->payload_length - config::HOSTNAME_LENGTH];
            uint32_t psw_ctr = 0;
            for(int i = 0; i < client->payload_length - config::HOSTNAME_LENGTH; i++){
                password[i] = buffer_pool_[client->reading_pointer];
                if(password[i] < 48
                || (password[i] > 57 && password[i] < 65)
                || (password[i] > 90 && password[i] < 95)
                || (password[i] > 95 && password[i] < 97)
                || password[i] > 122){
                    authentication_message_[8] = auth::INVALID_CREDENTIAL;
                    return sendStatusMessage(
                        client_socket,
                        client_socket,
                        authentication_message_,
                        config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH
                    );
                }
                psw_ctr++;
                client->advanceReadingPointer();
            }

            if(psw_ctr < config::MIN_PASSWORD_LENGTH || psw_ctr > config::MAX_PASSWORD_LENGTH){
                authentication_message_[8] = auth::INVALID_CREDENTIAL;
                return sendStatusMessage(
                    client_socket,
                    client_socket,
                    authentication_message_,
                    config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH
                );
            }

            // unique username
            if(username_to_client_key_.getDataCount() != 0){
                username_to_client_key_.resetNodeIndex();
                while(username_to_client_key_.hasNodes()){
                    if(username_to_client_key_.hasNode()){
                        bool equal_usernames = true;
                        char *ref_username = username_to_client_key_.getNode()->data_.username;
                        for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                            if(ref_username[i] != username[i]){
                                equal_usernames = false;
                                break;
                            }
                        }
                        if(equal_usernames){
                            authentication_message_[8] = auth::NOT_UNIQUE;
                            return sendStatusMessage(
                                client_socket,
                                client_socket,
                                authentication_message_,
                                config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH
                            );
                        }
                    }
                    username_to_client_key_.advanceNode();
                }
            }
            // get key
            client->sender_key = next_client_key_;
            if(next_client_key_ >= UINT32_MAX){
                return Status::EXCEEDED_CLIENT_MAX;
            }
            next_client_key_++;
            UsernameMapping userMapping;
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                client->name[i] = username[i];
                userMapping.username[i] = username[i];
            }
            userMapping.key = client->sender_key;

            if(!username_to_client_key_.insertNode(stringHash(client->name), userMapping)){
                return Status::ERROR;
            }
            if(!client_key_to_socket_.insertNode(client->sender_key, client_socket)){
                return Status::ERROR;
            }
            client->logged_in = true;

            LinkedList<uint32_t> *known_users = new(std::nothrow) LinkedList<uint32_t>;
            if(known_users == nullptr){
                return Status::ERROR;
            }
            client_key_to_known_keys_.insertNode(client->sender_key, known_users);
            if(!printClientInformation(client_socket)){
                return Status::ERROR;
            }
            authentication_message_[8] = auth::VALID;
            return sendStatusMessage(
                client_socket,
                client_socket,
                authentication_message_,
                config::HEADER_SIZE + config::AUTH_PAYLOAD_LENGTH
            );
        } break;
        case types::LOGIN:{
            // implement eventually
        } break;
        case types::SEND_REQUEST:{
            if(!client->logged_in){
                return Status::UNAUTHENTICATED_USER;
            }
            // search username. Verify that the connection is not established, if yes, just return true without doing anything.
            if(client->payload_length != config::HOSTNAME_LENGTH){
                return Status::INVALID_MESSAGE;
            }
            uint8_t target_username [config::HOSTNAME_LENGTH];
            uint32_t usr_ctr = 0;
            for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                target_username[i] = buffer_pool_[client->reading_pointer];
                if((target_username[i] > 0 && target_username[i] < 48)
                || (target_username[i] > 57 && target_username[i] < 65)
                || (target_username[i] > 90 && target_username[i] < 95)
                || (target_username[i] > 95 && target_username[i] < 97)
                || target_username[i] > 122){
                    return Status::INVALID_MESSAGE;
                }
                usr_ctr++;
                client->advanceReadingPointer();
            }
            if(usr_ctr < 1){
                return Status::INVALID_CLIENT;
            }
            if(username_to_client_key_.getDataCount() == 0){
                return Status::INVALID_CLIENT;
            }
            char *client_username;
            uint32_t client_key;
            username_to_client_key_.resetNodeIndex();
            while(username_to_client_key_.hasNodes()){
                if(username_to_client_key_.hasNode()){
                    bool equal_usernames = true;
                    client_username = username_to_client_key_.getNode()->data_.username;
                    client_key = username_to_client_key_.getNode()->data_.key;
                    client->receiver_key = client_key;
                    for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                        if(client_username[i] != target_username[i]){
                            equal_usernames = false;
                            break;
                        }
                    }
                    if(equal_usernames){
                        int *client_fd = client_key_to_socket_.getNode(client_key);
                        if(client_fd == nullptr){
                            return Status::ERROR;
                        }
                        client->receiver_fd = *client_fd;
                        for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
                            request_communication_message_[i + 2] = client->sender_key << ((config::CLIENT_KEY_LENGTH - 1 - i) * 8);
                        }
                        for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                            request_communication_message_[i + config::HEADER_SIZE] = client->name[i];
                        }
                        return sendStatusMessage(
                            client_socket,
                            client->receiver_fd,
                            request_communication_message_,
                            config::HEADER_SIZE + config::HOSTNAME_LENGTH
                        );
                    }
                }
                username_to_client_key_.advanceNode();
            }
            return Status::INVALID_CLIENT;
        } break;
        case types::ACCEPT_REQUEST:
        case types::REJECT_REQUEST:{
            if(client->payload_length != config::HOSTNAME_LENGTH){
                return Status::INVALID_MESSAGE;
            }
            if(username_to_client_key_.getDataCount() == 0 || !clients_.searchNode(client->receiver_fd)){
                return Status::INVALID_CLIENT;
            }
            Status send_state = sendToClient(client_socket);

            switch(send_state){
                case Status::RESOURCE_UNAVAILABLE:{
                    //should not return, rather be stored
                    return Status::RESOURCE_UNAVAILABLE;
                } break;
                case Status::ERROR:{
                    return Status::ERROR;
                } break;
            }

            if(client->type == types::ACCEPT_REQUEST){
                if(!client_key_to_known_keys_.searchNode(client->sender_key)){
                    return Status::ERROR;
                }
                LinkedList<uint32_t> *known_users = *client_key_to_known_keys_.getNode(client->sender_key);
                known_users->insertHead(client->receiver_key);

                if(!client_key_to_known_keys_.searchNode(client->receiver_key)){
                    return Status::ERROR;
                }
                known_users = *client_key_to_known_keys_.getNode(client->receiver_key);
                known_users->insertHead(client->sender_key);
            }
            // working here

            return Status::SUCCESS;
        } break;
        case types::ACK:{
            if(!client->logged_in){
                return Status::UNAUTHENTICATED_USER;
            }
            if(client_key_to_known_keys_.searchNode(client->sender_key)){
                LinkedList<uint32_t> *known_users = *client_key_to_known_keys_.getNode(client->sender_key);
                if(!known_users->searchNode(client->receiver_key)){
                    return Status::INVALID_CLIENT;
                }
            } else{
                return Status::ERROR;
            }
            if(!clients_.searchNode(client->receiver_fd)){
                return Status::INVALID_CLIENT;
            }
            delivered_ack_message_[2] = client->sender_key >> 24;
            delivered_ack_message_[3] = client->sender_key >> 16;
            delivered_ack_message_[4] = client->sender_key >> 8;
            delivered_ack_message_[5] = client->sender_key;
            Status ack_state = sendStatusMessage(
                client_socket,
                client->receiver_fd,
                delivered_ack_message_,
                config::HEADER_SIZE
            );
            switch(ack_state){
                case Status::RESOURCE_UNAVAILABLE:{
                    // should not return, rather be stored
                    return Status::RESOURCE_UNAVAILABLE;
                } break;
                case Status::ERROR:{
                    return Status::ERROR;
                } break;
            }
            return Status::SUCCESS;

        } break;
        default:{
            return Status::INVALID_MESSAGE;
        }
    }
    return Status::SUCCESS;
}

// Resets client buffer segments indicators to process new messages.
bool Server::cleanClientBuffer(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return false;
    }
    int dif = client->writing_buffer - client->starting_buffer;
    if(dif < 0){
        dif *= -1;
    }
    for(int i = 0; i < dif; i++){
        available_buffers_.insertHead(client->buffer_pointers[(client->starting_buffer + i) % config::BUFFER_SEGMENTS_PER_CLIENT]);
        client->buffer_pointers[(client->starting_buffer + i) % config::BUFFER_SEGMENTS_PER_CLIENT] = UINT32_MAX;
        client->buffer_pointers_count--;
    }
    client->starting_buffer = client->writing_buffer;
    client->reading_buffer = client->writing_buffer;

    client->starting_pointer = client->reading_pointer;
    client->byte_counter -= client->payload_length + config::HEADER_SIZE;
    client->resetMessage();
    return true;
}

/*
Sends different status messages to clients.
Returns INVALID_CLIENT, ERROR, RESOURCE_UNAVAILABLE, SUCCESS.
*/
Status Server::sendStatusMessage(int client_socket, int receiver_fd, uint8_t *buffer, int bytes_to_send){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::INVALID_CLIENT;
    }
    int total_bytes_sent = 0;
    int bytes_sent = 0;

    while(total_bytes_sent < bytes_to_send){
        if((bytes_sent = send(
            receiver_fd,
            &buffer[total_bytes_sent],
            bytes_to_send - total_bytes_sent,
            0)) == -1){
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return Status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of authentication failed.");
                return Status::ERROR;
            }
        }
        total_bytes_sent += bytes_sent;
    }
    return Status::SUCCESS;
}

/*
Sends message from one client to another.
Returns INVALID_CLIENT, ERROR, RESOURCE_UNAVAILABLE, SUCCESS.
*/
Status Server::sendToClient(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::INVALID_CLIENT;
    }
    int sending_pointer = 0;
    int bytes_to_send = client->payload_length + config::HEADER_SIZE;
    int total_bytes_sent = 0;

    client->reading_pointer = client->starting_pointer;
    for(int i = 0; i <  bytes_to_send; i++){
        sending_buffer_[sending_pointer] = buffer_pool_[client->reading_pointer];
        client->advanceReadingPointer();
        sending_pointer++;
    }
    while(total_bytes_sent < bytes_to_send){
        int sent_bytes = 0;
        if((sent_bytes = send(
            client->receiver_fd,
            &sending_buffer_[total_bytes_sent],
            (bytes_to_send - total_bytes_sent),
            0)) == -1)
        {
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                return Status::RESOURCE_UNAVAILABLE;
            } else{
                perror("Send of message failed.");
                return Status::ERROR;
            }
        } else{
            total_bytes_sent += sent_bytes;
        }
    }
    return Status::SUCCESS;
}

// Prints the Name, Key, IP, Port and Socket of a new client.
bool Server::printClientInformation(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return false;
    }
    std::cout
    << "Name: " << client->name << std::endl
    << "Key: " << client->sender_key << std::endl
    << "IP: " << client->ip << std::endl
    << "Port: " << client->port << std::endl
    << "Socket: " << client_socket << std::endl
    << std::endl;
    client = nullptr;
    return true;
}

unsigned long Server::stringHash(const char *str){
    unsigned long hash = 5381;
    int c;
    while (c = *str++){
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}