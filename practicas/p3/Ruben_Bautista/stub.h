#ifndef STUB_H
#define STUB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>

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

int wait_for_client_connection(int server_socket, int timeout_sec, int *running);
void close_connection(int socket);

#endif