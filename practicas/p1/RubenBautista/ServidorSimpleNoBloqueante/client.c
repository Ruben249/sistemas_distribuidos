/*
 * Non-blocking TCP Client - Systems Distributed and Concurrent
 * Client with non-blocking communication and 0.5s timeout
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024
#define CLIENT_TIMEOUT_US 500000  /* 0.5 seconds in microseconds */

/* Global variable for signal handler */
int client_socket = -1;

/* Function prototypes */
void handle_signal(int sig);
int setup_client_socket(void);
void handle_server_communication(int sock_fd);

/* Handle CTRL+C signal for graceful shutdown */
void handle_signal(int sig) {
    (void)sig;
    printf("\nDisconnecting from server...\n");
    if (client_socket != -1) {
        close(client_socket);
    }
    exit(0);
}

/* Set up and connect client socket */
int setup_client_socket(void) {
    struct sockaddr_in server_addr;
    int sock_fd;
    
    /* Create TCP socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }
    
    /* Configure server address structure */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    /* Convert IP address from text to binary form */
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        return -1;
    }
    
    /* Connect to server */
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("connect");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
}

/* Handle communication with server using non-blocking recv with timeout */
void handle_server_communication(int sock_fd) {
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_fd, activity;
    struct timeval timeout;
    
    printf("> ");
    fflush(stdout);
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sock_fd, &read_fds);
        
        max_fd = (STDIN_FILENO > sock_fd) ? STDIN_FILENO : sock_fd;
        
        /* Set timeout for select - 0.5 seconds as required */
        timeout.tv_sec = 0;
        timeout.tv_usec = CLIENT_TIMEOUT_US;
        
        /* Wait for activity on input descriptors with timeout */
        activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("select");
            break;
        }
        
        /* Handle incoming message from server with NON-BLOCKING recv */
        if (FD_ISSET(sock_fd, &read_fds)) {
            int bytes_received = recv(sock_fd, buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("\n+++ %s\n", buffer);
            } else if (bytes_received == 0) {
                printf("Server disconnected\n");
                break;
            } else {
                /* No data available or error */
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("recv");
                    break;
                }
            }
        }
        
        /* Handle user input */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = 0;
                
                if (send(sock_fd, buffer, strlen(buffer), 0) == -1) {
                    perror("send");
                    break;
                }
            }
        }
        
        /* Always show prompt after each iteration (non-blocking behavior) */
        printf("> ");
        fflush(stdout);
    }
}

int main() {
    /* Set output buffering for immediate display */
    setbuf(stdout, NULL);
    
    /* Register signal handler for CTRL+C */
    signal(SIGINT, handle_signal);
    
    /* Set up client socket and connect to server */
    client_socket = setup_client_socket();
    if (client_socket == -1) {
        exit(EXIT_FAILURE);
    }
    printf("Socket successfully created...\n");
    printf("Connected to the server...\n");
    
    /* Handle communication with server */
    handle_server_communication(client_socket);
    
    /* Cleanup */
    close(client_socket);
    return 0;
}