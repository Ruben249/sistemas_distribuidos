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
    
    while (1) {
        /* Get user input and send first (cliente empieza) */
        printf("> ");
        fflush(stdout);
        
        if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            
            if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
                perror("send");
                break;
            }
            
            /* Then wait for server response */
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_received <= 0) {
                printf("Server disconnected\n");
                break;
            }
            
            buffer[bytes_received] = '\0';
            printf("+++ %s\n", buffer);
        }
    }
    
    close(client_socket);
    return 0;
}