#include "stub.h"

// initialize_server_socket(): Initializes a server socket
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

// wait_for_client_with_select(): Waits for client connection using select
int wait_for_client_connection(int server_socket, int timeout_sec, volatile int *running) {
    struct timeval timeout;
    fd_set read_fds;
    int ready;
    int total_timeout = timeout_sec * 1000;
    int step_timeout = 100;

    while (total_timeout > 0 && *running) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        timeout.tv_sec = 0;
        timeout.tv_usec = step_timeout * 1000;

        ready = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        
        // Check the result of select. If interrupted, continue; if error, sleep and retry
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                usleep(step_timeout * 1000);
            }
        } else if (ready > 0) {
            if (FD_ISSET(server_socket, &read_fds)) {
                int client_socket = accept_client_connection(server_socket);
                if (client_socket < 0) {
                    if (errno != EINTR) {
                        usleep(step_timeout * 1000);
                    }
                    return -1;
                }
                return client_socket;
            }
        }

        total_timeout -= step_timeout;
    }

    return -1;
}
// accept_client_connection(): Accepts an incoming client connection
int accept_client_connection(int server_socket) {
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
    return client_socket;
}

// connect_to_server(): Connects to a server given its IP and port
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

// send_request(): Sends a request structure over a socket
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

// receive_request(): Receives a request structure from a socket
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

// send_response(): Sends a response structure over a socket
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

// receive_response(): Receives a response structure from a socket
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

// close_connection(): Closes a socket connection
void close_connection(int socket) {
    if (socket >= 0) {
        close(socket);
    }
}