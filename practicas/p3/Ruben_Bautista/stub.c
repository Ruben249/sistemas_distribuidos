#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "stub.h"

int server_socket_fd;
int client_socket_fd;
struct sockaddr_in server_address;
struct sockaddr_in client_address;

int initialize_server_socket(int port) { return 0; }
int accept_client_connection(int server_socket) { return 0; }
void close_server_socket(int server_socket) {}
int connect_to_server(char *server_ip, int port) { return 0; }
void close_client_connection(int client_socket) {}
int send_request(int socket_fd, struct request *req) { return 0; }
int receive_request(int socket_fd, struct request *req) { return 0; }
int send_response(int socket_fd, struct response *resp) { return 0; }
int receive_response(int socket_fd, struct response *resp) { return 0; }
int set_socket_reuse_option(int socket_fd) { return 0; }