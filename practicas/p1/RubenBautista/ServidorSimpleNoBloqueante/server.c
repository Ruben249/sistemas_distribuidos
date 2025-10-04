/*
 * Non-blocking TCP Server - Systems Distributed and Concurrent
 * Monoclient server with non-blocking communication
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

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1

/* Global variables for signal handler */
int server_socket = -1;
int client_socket = -1;

/* Function prototypes */
void handle_signal(int sig);
int setup_server_socket(void);
int accept_client_connection(int server_fd);
void handle_client_communication(int client_fd);
void cleanup_resources(void);

/* Handle CTRL+C signal for graceful shutdown */
void handle_signal(int sig) {
    (void)sig;
    printf("\nShutting down server...\n");
    cleanup_resources();
    exit(0);
}

/* Clean up all resources */
void cleanup_resources(void) {
    if (client_socket != -1) {
        close(client_socket);
    }
    if (server_socket != -1) {
        close(server_socket);
    }
}

/* Set up and configure server socket */
int setup_server_socket(void) {
    struct sockaddr_in server_addr;
    int sock_fd;
    const int enable = 1;
    
    /* Create TCP socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }
    
    /* Set socket option to reuse address */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt");
        close(sock_fd);
        return -1;
    }
    
    /* Configure server address structure */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    /* Bind socket to specific port */
    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }
    
    /* Listen for incoming connections */
    if (listen(sock_fd, MAX_CLIENTS) != 0) {
        perror("listen");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
}

/* Accept a client connection */
int accept_client_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;
    
    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
        perror("accept");
        return -1;
    }
    
    return client_fd;
}

/* Handle communication with a connected client using non-blocking recv */
void handle_client_communication(int client_fd) {
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_fd, activity;
    int client_connected = 1;
    
    printf("> ");
    fflush(stdout);
    
    while (client_connected) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(client_fd, &read_fds);
        
        max_fd = (STDIN_FILENO > client_fd) ? STDIN_FILENO : client_fd;
        
        /* Wait for activity on input descriptors - no timeout for server */
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (activity < 0) {
            perror("select");
            break;
        }
        
        /* Handle incoming message from client with NON-BLOCKING recv */
        if (FD_ISSET(client_fd, &read_fds)) {
            int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("\n+++ %s\n", buffer);
                printf("> ");
                fflush(stdout);
            } else if (bytes_received == 0) {
                printf("Client disconnected\n");
                client_connected = 0;
                break;
            } else {
                /* No data available - show prompt again */
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("> ");
                    fflush(stdout);
                } else {
                    perror("recv");
                    client_connected = 0;
                    break;
                }
            }
        }
        
        /* Handle user input */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            printf("> ");
            fflush(stdout);
            
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = 0;
                
                if (send(client_fd, buffer, strlen(buffer), 0) == -1) {
                    perror("send");
                    client_connected = 0;
                    break;
                }
            }
        }
    }
}

int main() {
    /* Set output buffering for immediate display */
    setbuf(stdout, NULL);
    
    /* Register signal handler for CTRL+C */
    signal(SIGINT, handle_signal);
    
    /* Main server loop - accept new clients after disconnections */
    while (1) {
        /* Set up server socket for each new client */
        server_socket = setup_server_socket();
        if (server_socket == -1) {
            exit(EXIT_FAILURE);
        }
        printf("Socket successfully created...\n");
        printf("Socket successfully binded...\n");
        printf("Server listening...\n");
        
        /* Accept client connection */
        client_socket = accept_client_connection(server_socket);
        if (client_socket == -1) {
            close(server_socket);
            continue;
        }
        
        /* Close listening socket to reject new connections while serving current client */
        close(server_socket);
        server_socket = -1;
        
        /* Handle communication with client */
        handle_client_communication(client_socket);
        
        /* Cleanup client connection and wait for new one */
        close(client_socket);
        client_socket = -1;
        printf("Waiting for new client connection...\n");
    }
    
    /* Cleanup (should not be reached due to signal handler) */
    cleanup_resources();
    return 0;
}