#ifndef STUB_H
#define STUB_H

enum operations {
    WRITE = 0,
    READ
};

struct request {
    enum operations action;
    unsigned int id;
};

struct response {
    enum operations action;
    unsigned int counter;
    long latency_time;
};

int initialize_server_socket(int port);
int accept_client_connection(int server_socket);
void close_server_socket(int server_socket);
int connect_to_server(char *server_ip, int port);
void close_client_connection(int client_socket);
int send_request(int socket_fd, struct request *req);
int receive_request(int socket_fd, struct request *req);
int send_response(int socket_fd, struct response *resp);
int receive_response(int socket_fd, struct response *resp);
int set_socket_reuse_option(int socket_fd);

#endif