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

// Server socket functions
int initialize_server_socket(int port);
int accept_client_connection(int server_socket);

// Client socket functions  
int connect_to_server(char *server_ip, int port);

// Communication functions
int send_request(int socket, struct request *req);
int receive_request(int socket, struct request *req);
int send_response(int socket, struct response *resp);
int receive_response(int socket, struct response *resp);

#endif