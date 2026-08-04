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
    memset(&info_message_, 0, sizeof(info_message_));
    memset(&processed_ack_message_, 0, sizeof(processed_ack_message_));
    memset(&delivered_ack_message_, 0, sizeof(delivered_ack_message_));
    memset(&request_communication_message_, 0, sizeof(request_communication_message_));
    memset(&accept_communication_message_, 0, sizeof(accept_communication_message_));
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
    try{
        if(!setupHashTables()){
            // wrong initial size
            return false;
        }
    } catch(const std::bad_alloc&){
        std::cout << "Error: insuficcient memory space!" << std::endl;
        return false;
    }

    try{
        setupBuffers();
    } catch(const std::bad_alloc&){
        std::cout << "Error: insuficcient memory space!" << std::endl;
        return false;
    }
    setupHeaderTypes();
    if(!setupListenerSocket()){
        return false;
    }
    return true;
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
    if(!client_key_to_requested_keys_.createTable(config::INITIAL_HASHTABLE_SIZE)){
        return false;
    }
    return true;
}

void Server::setupBuffers(){
    buffer_pool_ = new uint8_t[config::BUFFER_SIZE];
    receiver_buffer_ = new uint8_t[config::BUFFER_READING_SIZE];
    sending_buffer_ = new uint8_t[config::READING_BUFFER_SIZE];
    uint32_t current_address = 0;
    for(int i = 0; i < config::TOTAL_BUFFER_SEGMENTS; i++){
        available_buffers_.insertTail(current_address);
        current_address += config::BUFFER_SEGMENT_SIZE;
    }
}

void Server::setupHeaderTypes(){
    info_message_[0] = UINT8_MAX;
    info_message_[1] = types::INFO;
    info_message_[2] = UINT8_MAX;
    info_message_[3] = UINT8_MAX;
    info_message_[4] = UINT8_MAX;
    info_message_[5] = UINT8_MAX;
    info_message_[6] = 0;
    info_message_[7] = config::INFO_MESSAGE_LENGTH - config::HEADER_SIZE;
    info_message_[8] = 0;

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

    request_communication_message_[0] = UINT8_MAX;
    request_communication_message_[1] = types::SEND_REQUEST;
    request_communication_message_[2] = UINT8_MAX;
    request_communication_message_[3] = UINT8_MAX;
    request_communication_message_[4] = UINT8_MAX;
    request_communication_message_[5] = UINT8_MAX;
    request_communication_message_[6] = 0;
    request_communication_message_[7] = config::HOSTNAME_LENGTH;
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
                        case Status::EXCEEDED_CLIENT_MAX:{
                            accept_loop = false;
                            // log the event. clear queue.
                        } break;
                        case Status::PROGRAMMING_ERROR:{
                            //Log programming error.
                            std::cout << "There's a code error on acceptConnection!" << std::endl;
                            return;
                        } break;
                        case Status::INSUFFICIENT_BUFFER_SPACE:{
                            std::cout << "Error in distribution of buffer segments found on acceptConnection!" << std::endl;
                            return;
                        } break;
                        case Status::INSUFFICIENT_MEMORY:{
                            return;
                        } break;
                        case Status::ERROR:{
                            std::cout << "Unhandled error." << std::endl;
                            return;
                        } break;
                    }
                }
            } else if (events_[i].events & EPOLLIN){
                int client_socket = events_[i].data.fd;
                bool receive_loop = true;
                while(receive_loop){
                    Status rcvf_state = receiveFromClient(client_socket);
                    switch(rcvf_state){
                        case Status::SUCCESS:{
                            Status check_state;
                            do{
                                check_state = messageProcessor(client_socket);
                            }while (check_state == Status::SUCCESS);

                            switch(check_state){
                                case Status::INCOMPLETE_MESSAGE:{
                                } break;
                                case Status::RESOURCE_UNAVAILABLE:{
                                    receive_loop = false;
                                } break;
                                case Status::EXCEEDED_CLIENT_MAX:{
                                    std::cout << "Message protocol requires upgrade! Capacity of registers users exceeded!";
                                    return;
                                } break;
                                case Status::INSUFFICIENT_BUFFER_SPACE:{
                                    std::cout << "Error in distribution of buffer segments found on messageProcessor!" << std::endl;
                                    return;
                                } break;
                                case Status::INSUFFICIENT_MEMORY:{
                                    return;
                                } break;
                                case Status::ERROR:{
                                    return;
                                } break;
                            }
                        } break;
                        case Status::NOTHING_TO_READ:{
                            receive_loop = false;
                        } break;
                        case Status::CLOSED_CONVERSATION:{
                            Status close_connection = closeConnection(client_socket);
                            switch(close_connection){
                                case Status::SUCCESS:{
                                    return; // current method of shutting down server.
                                } break;
                                case Status::PROGRAMMING_ERROR:{
                                    std::cout << "There's a code error on closeConnection!" << std::endl;
                                    return;
                                } break;
                                case Status::INSUFFICIENT_MEMORY:{
                                    return;
                                } break;
                                case Status::ERROR:{
                                    return;
                                } break;
                            }
                        } break;
                        case Status::EXCEEDED_CLIENT_BUFFER_SIZE:{
                            bool valid_message = false;

                            Status check_state = messageProcessor(client_socket);
                            while (check_state == Status::SUCCESS){
                                valid_message = true;
                                check_state = messageProcessor(client_socket);
                                switch(check_state){
                                    case Status::INCOMPLETE_MESSAGE:{
                                    } break;
                                    case Status::RESOURCE_UNAVAILABLE:{
                                        receive_loop = false;
                                    } break;
                                    case Status::EXCEEDED_CLIENT_MAX:{
                                        std::cout << "Message protocol requires upgrade! Capacity of registers users exceeded!";
                                        return;
                                    } break;
                                    case Status::INSUFFICIENT_BUFFER_SPACE:{
                                        std::cout << "Error in distribution of buffer segments found on messageProcessor!" << std::endl;
                                        return;
                                    } break;
                                    case Status::INSUFFICIENT_MEMORY:{
                                        return;
                                    } break;
                                    case Status::ERROR:{
                                        return;
                                    } break;
                                }
                            }
                            if(valid_message){
                                break;
                            }
                            switch(check_state){
                                case Status::INCOMPLETE_MESSAGE:{
                                    info_message_[8] = info::INVALID_MESSAGE;
                                    Status send_state = sendMessage(
                                        client_socket,
                                        info_message_,
                                        config::INFO_MESSAGE_LENGTH
                                    );
                                    switch(send_state){
                                        case Status::RESOURCE_UNAVAILABLE:{
                                            //
                                        }
                                        case Status::ERROR:{
                                            return;
                                        }
                                    }
                                    Status reset_client_status = resetClientBuffer(client_socket);
                                    switch(reset_client_status){
                                        case Status::INSUFFICIENT_BUFFER_SPACE:{
                                            std::cout << "Error in distribution of buffer segments found on messageProcessor!" << std::endl;
                                            return;
                                        } break;
                                        case Status::INSUFFICIENT_MEMORY:{
                                            return;
                                        } break;
                                        case Status::ERROR:{
                                            return;
                                        } break;
                                    }
                                } break;
                                case Status::RESOURCE_UNAVAILABLE:{
                                    receive_loop = false;
                                } break;
                                case Status::EXCEEDED_CLIENT_MAX:{
                                    std::cout << "Message protocol requires upgrade! Capacity of registers users exceeded!";
                                    return;
                                } break;
                                case Status::INSUFFICIENT_BUFFER_SPACE:{
                                    std::cout << "Error in distribution of buffer segments found on messageProcessor!" << std::endl;
                                    return;
                                } break;
                                case Status::INSUFFICIENT_MEMORY:{
                                    return;
                                } break;
                                case Status::ERROR:{
                                    return;
                                } break;
                            }
                        } break;
                        case Status::INSUFFICIENT_BUFFER_SPACE:{
                            std::cout << "Error in distribution of buffer segments found on receiveFromClient!" << std::endl;
                            return;
                        } break;
                        case Status::ERROR:{
                            return;
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

/*
Accepts a incoming connection request and adds them as as a client.

SUCCESS - Client added correctly.
NOTHING_TO_DO - No pending clients to add.
EXCEEDED_CLIENT_MAX - Can't add more clients right now.
PROGRAMMING_ERROR - There's an issue with how hash tables are handled.
INSUFFICIENT_BUFFER_SPACE - Non available client buffer segments. Error in server code.
INSUFFICIENT_MEMORY - No memory available.
ERROR - Error that has not yet been handled.
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
            perror("Accept failed.");
            return Status::ERROR;
        }
    }
    Status addClientStatus = addClient(client_sockaddr);
    if(addClientStatus != Status::SUCCESS){
        return addClientStatus;
    }
    if(fcntl(pending_client_fd_, F_SETFL, O_NONBLOCK) == -1){
        perror("Non blocking failed");
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

/*
Registers a client with their socket info. Gives each registered client a buffer segment to occupy.

SUCCESS - Client added correctly.
PROGRAMMING_ERROR - There's an issue with how hash tables are handled.
INSUFFICIENT_BUFFER_SPACE - Non available client buffer segments. Error in server code.
INSUFFICIENT_MEMORY - No memory available.
*/
Status Server::addClient(const sockaddr_storage& client_sockaddr){
    Client new_client;
    new_client.name[0] = '\0';
    void* addr;
    if(client_sockaddr.ss_family == AF_INET){
        sockaddr_in* ipv4 = (sockaddr_in*)&client_sockaddr;
        addr = &(ipv4->sin_addr);
        new_client.port = ntohs(ipv4->sin_port);
    }
    else{
        sockaddr_in6* ipv6 = (sockaddr_in6*)&client_sockaddr;
        addr = &(ipv6->sin6_addr);
        new_client.port = ntohs(ipv6->sin6_port);
    }
    inet_ntop(client_sockaddr.ss_family, addr, new_client.ip, sizeof(new_client.ip));
    if(available_buffers_.isEmpty()){
        return Status::INSUFFICIENT_BUFFER_SPACE;
    }
    new_client.buffer_pointers[0] = available_buffers_.getHead();
    available_buffers_.deleteHead();
    new_client.buffer_pointers_count = 1;
    new_client.starting_pointer = new_client.buffer_pointers[0];
    new_client.reading_pointer = new_client.buffer_pointers[0];
    new_client.writing_pointer = new_client.buffer_pointers[0];

    try{
        if(!clients_.insertNode(pending_client_fd_, new_client)){
            return Status::PROGRAMMING_ERROR;
        }
    } catch (const std::bad_alloc&){
        return Status::INSUFFICIENT_MEMORY;
    }
    return Status::SUCCESS;
}

/*
Closes a client connection. Returns occupied buffer segments to the buffer pool.

SUCCESS - Socket closed correctly.
PROGRAMMING_ERROR - There's an issue with the deleting of nodes in hash table.
INSUFFICIENT_MEMORY - No memory available.
ERROR - Error that has not yet been handled.
*/
Status Server::closeConnection(int client_socket){
    if(close(client_socket) == -1){
        perror("clossing failed");
        return Status::ERROR;
    }
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::ERROR;
    }
    int8_t buffers_erased = 0;
    for(int i = 0; i < config::BUFFER_SEGMENTS_PER_CLIENT; i++){
        if(buffers_erased >= client->buffer_pointers_count){
            break;
        }
        if(client->buffer_pointers[i] != UINT32_MAX){
            try{
                available_buffers_.insertHead(client->buffer_pointers[i]);
            } catch(const std::bad_alloc&){
                return Status::INSUFFICIENT_MEMORY;
            }
            buffers_erased++;
        }
    }
    if(client->sender_key != UINT32_MAX){
        if(!client_key_to_socket_.deleteNode(client->sender_key)){
            return Status::PROGRAMMING_ERROR;
        }
        if(!username_to_client_key_.deleteNode(stringHash(client->name))){
            return Status::PROGRAMMING_ERROR;
        }
    }
    if(!clients_.deleteNode(client_socket)){
        return Status::PROGRAMMING_ERROR;
    }
    std::cout << "manual close of socket "  << client_socket << std::endl;
    return Status::SUCCESS;
}

/*
Copies an incoming message (possibly fragmented) to the corresponding client buffers.

SUCCESS - All bytes were received and copied to client buffers.
NOTHING_TO_READ - There are no more bytes to copy from a client.
CLOSED_CONVERSATION - The client closed the conversation.
EXCEEDED_CLIENT_BUFFER_SIZE - All client buffers have been filled.
INSUFFICIENT_BUFFER_SPACE - Non available client buffer segments. Error in server code.
ERROR - Error that has not yet been handled.
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
    //copy received messages to client buffers.
    int bytes_remaining = bytes_received;
    int msg_buffer_offset = 0;
    while(bytes_remaining > 0){
        uint32_t available_segment_bytes = client->getRemainingBytesWriting();
        if(available_segment_bytes > bytes_remaining){
            memcpy(&buffer_pool_[client->writing_pointer], &receiver_buffer_[msg_buffer_offset], bytes_remaining);
            client->writing_pointer += bytes_remaining;
            client->byte_counter += bytes_remaining;
            bytes_remaining = 0;
        }else{
            memcpy(&buffer_pool_[client->writing_pointer], &receiver_buffer_[msg_buffer_offset], available_segment_bytes);
            client->byte_counter += available_segment_bytes;
            msg_buffer_offset += available_segment_bytes;
            bytes_remaining -= available_segment_bytes;
            if(client->buffer_pointers_count + 1 >= config::BUFFER_SEGMENTS_PER_CLIENT){
                return Status::EXCEEDED_CLIENT_BUFFER_SIZE;
            }
            if(available_buffers_.isEmpty()){
                return Status::INSUFFICIENT_BUFFER_SPACE;
            }
            uint32_t new_buffer_segment = available_buffers_.getHead();
            client->buffer_pointers[(client->writing_buffer + 1) % 128] = new_buffer_segment;
            available_buffers_.deleteHead();
            client->buffer_pointers_count++;
            client->writing_buffer = (client->writing_buffer + 1) % 128;
            client->writing_pointer = new_buffer_segment;
        }
    }
    return Status::SUCCESS;
}

/*
Checks message and acts on it based on its type.

SUCCESS - Message is complete and has been processed.
INCOMPLETE_MESSAGE - The entire message has not been received.
RESOURCE_UNAVAILABLE - Unexpected error.
EXCEEDED_CLIENT_MAX - No more users can be registered.
INSUFFICIENT_BUFFER_SPACE - Non available client buffer segments. Error in server code.
INSUFFICIENT_MEMORY - No memory available.
ERROR - An unhandled error ocurred.
*/
Status Server::messageProcessor(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::ERROR;
    }
    Status check_state = checkMessage(client_socket);
    switch(check_state){
        case Status::INCOMPLETE_MESSAGE:{
            return Status::INCOMPLETE_MESSAGE;
        } break;
        case Status::INVALID_CLIENT:
        case Status::INVALID_MESSAGE:{
            if(check_state == Status::INVALID_CLIENT){
                info_message_[8] = info::INVALID_CLIENT;
            } else{
                info_message_[8] = info::INVALID_MESSAGE;
            }
            sendMessage(
                client_socket,
                info_message_,
                config::INFO_MESSAGE_LENGTH
            );
            Status reset_client_status = resetClientBuffer(client_socket);
            switch(reset_client_status){
                case Status::INSUFFICIENT_BUFFER_SPACE:{
                    return Status::INSUFFICIENT_BUFFER_SPACE;
                } break;
                case Status::INSUFFICIENT_MEMORY:{
                    return Status::INSUFFICIENT_MEMORY;
                } break;
                case Status::ERROR:{
                    return Status::ERROR;
                } break;
            }
            return Status::SUCCESS;
        } break;
        case Status::ERROR:{
            return Status::ERROR;
        } break;
    }

    Status act_state = actOnMessage(client_socket);
    switch(act_state){
        case Status::SUCCESS:{
            if(!cleanClientBuffer(client_socket)){
                return Status::ERROR;
            }
            return Status::SUCCESS;
        }
        case Status::INVALID_CLIENT:{
            info_message_[8] = info::INVALID_CLIENT;
            Status send_status = sendMessage(
                client_socket,
                info_message_,
                config::INFO_MESSAGE_LENGTH
            );
            if(!cleanClientBuffer(client_socket)){
                return Status::ERROR;
            }
            return send_status;
        }
        case Status::INVALID_MESSAGE:{
            Status send_status = sendMessage(
                client_socket,
                info_message_,
                config::INFO_MESSAGE_LENGTH
            );
            if(!cleanClientBuffer(client_socket)){
                return Status::ERROR;
            }
            return send_status;
        } break;
        case Status::RESOURCE_UNAVAILABLE:{
            info_message_[8] = info::SEND_ERROR;
            Status send_status = sendMessage(
                client_socket,
                info_message_,
                config::INFO_MESSAGE_LENGTH
            );
            if(!cleanClientBuffer(client_socket)){
                return Status::ERROR;
            }
            return Status::RESOURCE_UNAVAILABLE;
        } break;
        case Status::EXCEEDED_CLIENT_MAX:{
            info_message_[8] = info::COULD_NOT_REGISTER;
            Status send_status = sendMessage(
                client_socket,
                info_message_,
                config::INFO_MESSAGE_LENGTH
            );
            if(!cleanClientBuffer(client_socket)){
                return Status::ERROR;
            }
            return Status::EXCEEDED_CLIENT_MAX;
        } break;
        case Status::ERROR:{
            return Status::ERROR;
        } break;
    }
    return Status::ERROR;
}

/*
Checks if the entire message header + payload have been received.

SUCCESS - Message header is complete and valid.
INCOMPLETE_MESSAGE - The entire message has not been received.
INVALID_CLIENT - The target client is non existant.
INVALID_MESSAGE - The message does not follow the protocol rules.
ERROR - An unhandled error ocurred.
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

SUCCESS - Message header is complete and valid.
INCOMPLETE_MESSAGE - The entire header has not been received.
INVALID_CLIENT - The target client is non existant.
INVALID_MESSAGE - The message does not follow the protocol rules.
ERROR - An unhandled error ocurred.
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

SUCCESS - The message was valid and was handled correctly.
INVALID_CLIENT - Client sent a message to client that does not exist, or they are not allowed to message to.
INVALID_MESSAGE - Client sent an invalid message.
RESOURCE_UNAVAILABLE - Unexpected error.
EXCEEDED_CLIENT_MAX - No more users can be registered.
ERROR - An unhandled error ocurred.
*/
Status Server::actOnMessage(int client_socket){
    Client* client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::ERROR;
    }

    if(client->logged_in == false && client->type != types::REGISTER && client->type != types::LOGIN){
        info_message_[8] = info::UNAUTHENTICATED_USER;
        return Status::INVALID_MESSAGE;
    }

    switch(client->type){
        case types::USER:{
            return actOnUserMessage(client_socket, client);
        } break;
        case types::REGISTER:{
            return actOnRegister(client_socket, client);
        } break;
        case types::SEND_REQUEST:{
            return actOnSendRequest(client_socket, client);
        } break;
        case types::ACCEPT_REQUEST:
        case types::REJECT_REQUEST:{
            return actOnRespondToRequest(client_socket, client);
        } break;
        case types::ACK:{
            return actOnAcknowledgement(client_socket, client);
        } break;
        default:{
            info_message_[8] = info::INVALID_MESSAGE;
            return Status::INVALID_MESSAGE;
        }
    }
    return Status::ERROR;
}

/*
Process a user message and determines if the message should be sent. Sends the message to the receiver.

SUCCESS - The message was valid and was sent.
INVALID_CLIENT - Client sent a message to a client that does not exist, or they are not allowed to message to.
INVALID_MESSAGE - Client sent an invalid message.
RESOURCE_UNAVAILABLE - Unexpected error.
ERROR - An unhandled error ocurred.
*/
Status Server::actOnUserMessage(int client_socket, Client *client){
    if(client->payload_length == 0 || client->payload_length > config::MAX_MESSAGE_SIZE){
        info_message_[8] = info::INVALID_MESSAGE;
        return Status::INVALID_MESSAGE;
    }
    // Sender knows receiver
    if(!client_key_to_known_keys_.searchNode(client->sender_key)){
        return Status::ERROR;
    }
    LinkedList<uint32_t> *known_users = *client_key_to_known_keys_.getNode(client->sender_key);
    if(!known_users->searchNode(client->receiver_key)){
        return Status::INVALID_CLIENT;
    }

    // later it should be changed to store all client keys, regardless of whether online or not.
    if(!clients_.searchNode(client->receiver_fd)){
        return Status::INVALID_CLIENT;
    }
    Status ack_state = sendMessage(
        client_socket,
        processed_ack_message_,
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
    // If client is not available it should be stored in some file. (much later)
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
    return Status::SUCCESS;
}

/*
Process a user registration attempt and reviews credentials structure. Registers the user.

SUCCESS - The registration request was valid.
INVALID_MESSAGE - Client sent an invalid registration request.
EXCEEDED_CLIENT_MAX - The server cannnot register more users.
RESOURCE_UNAVAILABLE - Unexpected error.
ERROR - An unhandled error ocurred.
*/
Status Server::actOnRegister(int client_socket, Client *client){
    if(client->logged_in){
        info_message_[8] = info::ALREADY_LOGGED_IN;
        return Status::INVALID_MESSAGE;
    }

    if(client->payload_length < config::HOSTNAME_LENGTH + config::MIN_PASSWORD_LENGTH
    || client->payload_length > config::HOSTNAME_LENGTH + config::MAX_PASSWORD_LENGTH){
        info_message_[8] = info::INVALID_CREDENTIAL;
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
            info_message_[8] = info::INVALID_CREDENTIAL;
            return Status::INVALID_MESSAGE;
        }
        usr_ctr += username[i] != 0 ? 1 : 0;
        client->advanceReadingPointer();
    }
    if(usr_ctr < 1){
        info_message_[8] = info::INVALID_CREDENTIAL;
        return Status::INVALID_MESSAGE;
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
            info_message_[8] = info::INVALID_CREDENTIAL;
            return Status::INVALID_MESSAGE;
        }
        psw_ctr++;
        client->advanceReadingPointer();
    }

    if(psw_ctr < config::MIN_PASSWORD_LENGTH || psw_ctr > config::MAX_PASSWORD_LENGTH){
        info_message_[8] = info::INVALID_CREDENTIAL;
        return Status::INVALID_MESSAGE;
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
                    info_message_[8] = info::NOT_UNIQUE;
                    return Status::INVALID_MESSAGE;
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
    LinkedList<uint32_t> *requests_to_users = new(std::nothrow) LinkedList<uint32_t>;
    if(requests_to_users == nullptr){
        return Status::ERROR;
    }
    client_key_to_requested_keys_.insertNode(client->sender_key, requests_to_users);
    if(!printClientInformation(client_socket)){
        return Status::ERROR;
    }
    info_message_[8] = info::VALID_REGISTER;
    return sendMessage(
        client_socket,
        info_message_,
        config::INFO_MESSAGE_LENGTH
    );
}

/*
Process a user registration attempt and reviews credentials structure. Registers the user.

SUCCESS - The request was valid and was sent to the receiver.
INVALID_CLIENT - Client sent an invalid connection request.
INVALID_MESSAGE - Client sent an invalid message.
RESOURCE_UNAVAILABLE - Unexpected error.
ERROR - An unhandled error ocurred.
*/
Status Server::actOnSendRequest(int client_socket, Client *client){
    // search username. Verify that the connection is not established, if yes, just return true without doing anything.
    if(client->payload_length != config::HOSTNAME_LENGTH){
        info_message_[8] = info::INVALID_MESSAGE;
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
            info_message_[8] = info::INVALID_MESSAGE;
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

                if(!client_key_to_known_keys_.searchNode(client->sender_key)){
                    return Status::ERROR;
                }
                LinkedList<uint32_t> *known_clients = *client_key_to_known_keys_.getNode(client->sender_key);
                if(known_clients->searchNode(client->receiver_key)){
                    info_message_[8] = info::ALREADY_KNOWN_CLIENT;
                    return Status::INVALID_MESSAGE;
                }

                if(!client_key_to_requested_keys_.searchNode(client->sender_key)){
                    return Status::ERROR;
                }
                LinkedList<uint32_t> *requests = *client_key_to_requested_keys_.getNode(client->sender_key);
                if(requests->searchNode(client->receiver_key)){
                    info_message_[8] = info::ALREADY_SENT_REQUEST;
                    return Status::INVALID_MESSAGE;
                }
                requests->insertHead(client->receiver_key);

                for(int i = 0; i < config::CLIENT_KEY_LENGTH; i++){
                    request_communication_message_[i + 2] = client->sender_key << ((config::CLIENT_KEY_LENGTH - 1 - i) * 8);
                }
                for(int i = 0; i < config::HOSTNAME_LENGTH; i++){
                    request_communication_message_[i + config::HEADER_SIZE] = client->name[i];
                }
                return sendMessage(
                    client->receiver_fd,
                    request_communication_message_,
                    config::HEADER_SIZE + config::HOSTNAME_LENGTH
                );
            }
        }
        username_to_client_key_.advanceNode();
    }
    return Status::INVALID_CLIENT;
}

/*
Process a user registration attempt and reviews credentials structure. Registers the user.

SUCCESS - The response was valid and was sent to the receiver.
INVALID_CLIENT - Client sent an invalid response.
INVALID_MESSAGE - Client sent an invalid message.
RESOURCE_UNAVAILABLE - Unexpected error.
ERROR - An unhandled error ocurred.
*/
Status Server::actOnRespondToRequest(int client_socket, Client *client){
    //validate if request exist
    if(client->payload_length != config::HOSTNAME_LENGTH){
        info_message_[8] = info::INVALID_MESSAGE;
        return Status::INVALID_MESSAGE;
    }
    if(!clients_.searchNode(client->receiver_fd)){
        return Status::INVALID_CLIENT;
    }

    if(!client_key_to_known_keys_.searchNode(client->sender_key)){
        return Status::ERROR;
    }
    LinkedList<uint32_t> *known_clients = *client_key_to_known_keys_.getNode(client->sender_key);
    if(known_clients->searchNode(client->receiver_key)){
        info_message_[8] = info::ALREADY_KNOWN_CLIENT;
        return Status::INVALID_MESSAGE;
    }

    if(!client_key_to_requested_keys_.searchNode(client->receiver_key)){
        return Status::ERROR;
    }
    LinkedList<uint32_t> *requests = *client_key_to_requested_keys_.getNode(client->receiver_key);
    if(!requests->searchNode(client->sender_key)){
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
        known_clients->insertHead(client->receiver_key);

        if(!client_key_to_known_keys_.searchNode(client->receiver_key)){
            return Status::ERROR;
        }
        known_clients = *client_key_to_known_keys_.getNode(client->receiver_key);
        known_clients->insertHead(client->sender_key);
    }
    return Status::SUCCESS;
}

/*
Process a user registration attempt and reviews credentials structure. Registers the user.

SUCCESS - The acknowledgement was valid and was sent to the receiver.
INVALID_CLIENT - Client sent an invalid acknowledgement.
INVALID_MESSAGE - Client sent an invalid acknowledgement.
RESOURCE_UNAVAILABLE - Unexpected error.
ERROR - An unhandled error ocurred.
*/
Status Server::actOnAcknowledgement(int client_socket, Client *client){
    if(client->payload_length != 0){
        info_message_[8] = info::INVALID_MESSAGE;
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
    }
    delivered_ack_message_[2] = client->sender_key >> 24;
    delivered_ack_message_[3] = client->sender_key >> 16;
    delivered_ack_message_[4] = client->sender_key >> 8;
    delivered_ack_message_[5] = client->sender_key;
    Status ack_state = sendMessage(
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
}

/*
Sends different status messages to clients.

SUCCESS - The entire status message has been sent.
RESOURCE_UNAVAILABLE - Unexpected error.
ERROR - An unhandled error ocurred.
*/
Status Server::sendMessage(int receiver_fd, uint8_t *buffer, int bytes_to_send){
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

SUCCESS - The entire message has been sent.
RESOURCE_UNAVAILABLE - Unexpected error.
ERROR - An unhandled error ocurred.
*/
Status Server::sendToClient(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::ERROR;
    }
    int sending_pointer = 0;
    int bytes_to_send = client->payload_length + config::HEADER_SIZE;

    client->reading_pointer = client->starting_pointer;
    for(int i = 0; i <  bytes_to_send; i++){
        sending_buffer_[sending_pointer] = buffer_pool_[client->reading_pointer];
        client->advanceReadingPointer();
        sending_pointer++;
    }

    return sendMessage(
        client->receiver_fd,
        sending_buffer_,
        bytes_to_send
    );
}

/*
Resets client buffers after an invalid message has been received.

SUCCESS - The entire message has been sent.
INSUFFICIENT_BUFFER_SPACE - Non available client buffer segments. Error in server code.
INSUFFICIENT_MEMORY - No memory available.
ERROR - An unhandled error ocurred.
*/
Status Server::resetClientBuffer(int client_socket){
    Client *client = clients_.getNode(client_socket);
    if(client == nullptr){
        return Status::ERROR;
    }
    for(int i = 0; i < config::BUFFER_SEGMENTS_PER_CLIENT; i++){
        if(client->buffer_pointers_count <= 0){
            break;
        }
        if(client->buffer_pointers[i] != UINT32_MAX){
            try{
                available_buffers_.insertHead(client->buffer_pointers[i]);
                client->buffer_pointers[i] = UINT32_MAX;
                client->buffer_pointers_count--;
            } catch(const std::bad_alloc&){
                return Status::INSUFFICIENT_MEMORY;
            }
        }
    }
    if(available_buffers_.isEmpty()){
        return Status::INSUFFICIENT_BUFFER_SPACE;
    }
    client->buffer_pointers[0] = available_buffers_.getHead();
    available_buffers_.deleteHead();
    client->buffer_pointers_count = 1;
    client->starting_buffer = 0;
    client->writing_buffer = 0;
    client->reading_buffer = 0;
    client->starting_pointer = client->buffer_pointers[0];
    client->writing_pointer = client->buffer_pointers[0];
    client->reading_pointer = client->buffer_pointers[0];
    client->byte_counter = 0;
    client->resetMessage();
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