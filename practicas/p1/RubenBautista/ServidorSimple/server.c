/*
 * Simple TCP Server - Systems Distributed and Concurrent
 * Monoclient server with bidirectional synchronous communication
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
int connection_socket = -1;
int state = 0;

/* Clean up all resources */
void cleanup_resources(void) {
    if (connection_socket != -1) {
        close(connection_socket);
    }
    if (server_socket != -1) {
        close(server_socket);
    }
}

/* Handle CTRL+C signal for graceful shutdown */
void handle_signal(int sig) {
    (void)sig;
    printf("\nShutting down server...\n");
    state = 1;
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

/* Handle communication with a connected client */
void handle_client_communication(int client_fd) {
    char buffer[BUFFER_SIZE];
    int client_connected = 1;
    
    while (client_connected && state == 0) {
        /* Wait for client message first (cliente empieza) - recv bloqueante */
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            printf("Client disconnected\n");
            client_connected = 0;
            break;
        }
        
        buffer[bytes_received] = '\0';
        printf("+++ %s\n", buffer);
        
        /* Check if we need to shutdown after recv */
        if (state == 1) {
            break;
        }
        
        /* Then get user input and send response */
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
        
        /* Check if we need to shutdown after send */
        if (state == 1) {
            break;
        }
    }
}

int main() {
    setbuf(stdout, NULL);
    
    /* Configure signal handling to interrupt blocking calls */
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  /* Important: restart system calls after signal */
    sigaction(SIGINT, &sa, NULL);
    
    while (state == 0) {
        /* Set up server socket for each new client */
        server_socket = setup_server_socket();
        if (server_socket == -1) {
            exit(EXIT_FAILURE);
        }
        printf("Socket successfully created...\n");
        printf("Socket successfully binded...\n");
        printf("Server listening...\n");

        
        /* Generate client connection */
        connection_socket = accept_client_connection(server_socket);
        if (connection_socket == -1) {
            close(server_socket);
            if (state == 1) break;
            continue;
        }
        
        /* Close listening socket to reject new connections while serving current client */
        close(server_socket);
        server_socket = -1;
        
        /* Handle communication with client */
        handle_client_communication(connection_socket);
        
        /* Cleanup client connection and wait for new one */
        close(connection_socket);
        connection_socket = -1;
        
        if (state == 0) {
            printf("Waiting for new client connection...\n");
        }
    }
    
    cleanup_resources();
    printf("Server shutdown complete.\n");
    return 0;
}