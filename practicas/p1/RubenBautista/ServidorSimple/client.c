/*
 * Simple TCP Client - Systems Distributed and Concurrent
 * Client for bidirectional synchronous communication with server
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

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

int client_socket = -1;

/* Handle CTRL+C signal */
void handle_signal(int sig) {
    (void)sig;
    printf("\nDisconnecting from server...\n");
    if (client_socket != -1) {
        close(client_socket);
    }
    exit(0);
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_fd, activity;
    
    /* Set output buffering for immediate display */
    setbuf(stdout, NULL);
    
    /* Register signal handler for CTRL+C */
    signal(SIGINT, handle_signal);
    
    /* Create TCP socket */
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket successfully created...\n");
    
    /* Configure server address structure */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    /* Connect to server */
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("connect");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    printf("Connected to the server...\n");
    
    printf("> ");
    fflush(stdout);
    
    /* Main communication loop */
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(client_socket, &read_fds);
        
        /* Find maximum file descriptor for select() */
        if (STDIN_FILENO > client_socket) {
            max_fd = STDIN_FILENO;
        } else {
            max_fd = client_socket;
        }
        
        /* Wait for activity on input descriptors */
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (activity < 0) {
            perror("select");
            break;
        }
        
        /* Handle incoming message from server */
        if (FD_ISSET(client_socket, &read_fds)) {
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_received == 0) {
                /* Server closed connection gracefully */
                printf("\nServer closed the connection\n");
                break;
            } else if (bytes_received < 0) {
                /* Error in recv */
                perror("recv");
                break;
            } else {
                /* Data received successfully */
                buffer[bytes_received] = '\0';
                printf("\n+++ %s\n", buffer);
            }
        }
        
        /* Handle user input */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            printf("> ");
            fflush(stdout);
            
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = 0;
                
                if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
                    perror("send");
                    break;
                }
            }
        }
        
        printf("> ");
        fflush(stdout);
    }
    
    close(client_socket);
    return 0;
}