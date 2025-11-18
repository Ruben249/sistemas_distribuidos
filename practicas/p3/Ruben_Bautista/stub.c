#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "stub.h"

// initialize_server_socket(): Creates and configures the server socket
int initialize_server_socket(int port) {
    int server_socket;
    struct sockaddr_in server_addr;
    const int enable = 1;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        return -1;
    }
    
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        close(server_socket);
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_socket);
        return -1;
    }
    
    if (listen(server_socket, 1024) < 0) {
        close(server_socket);
        return -1;
    }
    
    return server_socket;
}

// accept_client_connection(): Accepts an incoming client connection
int accept_client_connection(int server_socket) {
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
    return client_socket;
}

// connect_to_server(): Establishes connection to the server from client
int connect_to_server(char *server_ip, int port) {
    int client_socket;
    struct sockaddr_in server_addr;
    
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        close(client_socket);
        return -1;
    }
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(client_socket);
        return -1;
    }
    
    return client_socket;
}

// send_request(): Sends a request to the server
int send_request(int socket, struct request *req) {
    char *request_ptr = (char *)req;
    int remaining_bytes = sizeof(struct request);
    int total_sent = 0;
    
    while (remaining_bytes > 0) {
        int bytes_sent = send(socket, request_ptr, remaining_bytes, MSG_NOSIGNAL);
        if (bytes_sent <= 0) {
            return -1;
        }
        request_ptr += bytes_sent;
        remaining_bytes -= bytes_sent;
        total_sent += bytes_sent;
    }
    
    return total_sent;
}

// receive_request(): Receives a request from client
int receive_request(int socket, struct request *req) {
    char *request_ptr = (char *)req;
    int remaining_bytes = sizeof(struct request);
    int total_received = 0;
    
    while (remaining_bytes > 0) {
        int bytes_received = recv(socket, request_ptr, remaining_bytes, 0);
        if (bytes_received <= 0) {
            return -1;
        }
        request_ptr += bytes_received;
        remaining_bytes -= bytes_received;
        total_received += bytes_received;
    }
    
    return total_received;
}

// send_response(): Sends a response to the client
int send_response(int socket, struct response *resp) {
    char *response_ptr = (char *)resp;
    int remaining_bytes = sizeof(struct response);
    int total_sent = 0;
    
    while (remaining_bytes > 0) {
        int bytes_sent = send(socket, response_ptr, remaining_bytes, MSG_NOSIGNAL);
        if (bytes_sent <= 0) {
            return -1;
        }
        response_ptr += bytes_sent;
        remaining_bytes -= bytes_sent;
        total_sent += bytes_sent;
    }
    
    return total_sent;
}

// receive_response(): The client receives a response from server 
int receive_response(int socket, struct response *resp) {
    char *response_ptr = (char *)resp;
    int remaining_bytes = sizeof(struct response);
    int total_received = 0;
    
    while (remaining_bytes > 0) {
        int bytes_received = recv(socket, response_ptr, remaining_bytes, 0);
        if (bytes_received <= 0) {
            return -1;
        }
        response_ptr += bytes_received;
        remaining_bytes -= bytes_received;
        total_received += bytes_received;
    }
    
    return total_received;
}