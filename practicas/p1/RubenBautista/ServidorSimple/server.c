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

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1

#ifdef DEBUG
    #define DEBUG_PRINTF(...) printf("DEBUG: " __VA_ARGS__)
#else
    #define DEBUG_PRINTF(...)
#endif

/* Global variables for signal handling */
int server_socket = -1;
int client_socket = -1;

/* Function prototypes */
void handle_signal(int sig);
int setup_server_socket(void);
int accept_client_connection(int server_fd);
void handle_client_communication(int client_fd);
void print_prompt(void);
void print_received_message(const char* message);

/* Signal handler for graceful shutdown */
void handle_signal(int sig) {
    (void)sig;  /* Avoid unused parameter warning */
    printf("\nShutting down server...\n");
    if (client_socket != -1) {
        close(client_socket);
    }
    if (server_socket != -1) {
        close(server_socket);
    }
    exit(0);
}

/* Set up and configure server socket */
int setup_server_socket(void) {
    struct sockaddr_in server_addr;
    int sock_fd, opt = 1;
    
    /* Create server socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation failed");
        return -1;
    }
    
    /* Set socket options to reuse address */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(sock_fd);
        return -1;
    }
    
    /* Configure server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    /* Bind socket to address */
    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("Socket bind failed");
        close(sock_fd);
        return -1;
    }
    
    /* Start listening for connections */
    if (listen(sock_fd, MAX_CLIENTS) != 0) {
        perror("Listen failed");
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
        perror("Accept failed");
        return -1;
    }
    
    printf("Client connected from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return client_fd;
}

/* Print prompt for user input */
void print_prompt(void) {
    printf("> ");
    fflush(stdout);
}

/* Print received message with proper formatting */
void print_received_message(const char* message) {
    printf("+++ %s\n", message);
}

/* Handle communication with a connected client */
void handle_client_communication(int client_fd) {
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_fd, activity;
    int client_connected = 1;
    
    print_prompt();
    
    while (client_connected) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(client_fd, &read_fds);
        
        max_fd = (STDIN_FILENO > client_fd) ? STDIN_FILENO : client_fd;
        
        /* Wait for activity on either input or socket */
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (activity < 0) {
            perror("Select error");
            break;
        }
        
        /* Handle incoming message from client */
        if (FD_ISSET(client_fd, &read_fds)) {
            int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_received <= 0) {
                printf("Client disconnected\n");
                client_connected = 0;
                break;
            }
            
            buffer[bytes_received] = '\0';
            print_received_message(buffer);
            DEBUG_PRINTF("Received from client: %s\n", buffer);
            print_prompt();
        }
        
        /* Handle user input */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = 0;  /* Remove newline */
                
                if (send(client_fd, buffer, strlen(buffer), 0) == -1) {
                    perror("Send failed");
                    client_connected = 0;
                    break;
                }
                DEBUG_PRINTF("Sent to client: %s\n", buffer);
                print_prompt();
            }
        }
    }
}

int main() {
    /* Set up signal handling */
    signal(SIGINT, handle_signal);
    
    /* Set up server socket */
    server_socket = setup_server_socket();
    if (server_socket == -1) {
        exit(EXIT_FAILURE);
    }
    printf("Socket successfully created...\n");
    printf("Socket successfully binded...\n");
    printf("Server listening...\n");
    
    /* Main server loop */
    while (1) {
        /* Accept client connection */
        client_socket = accept_client_connection(server_socket);
        if (client_socket == -1) {
            continue;
        }
        
        /* Handle communication with client */
        handle_client_communication(client_socket);
        
        /* Cleanup client connection */
        close(client_socket);
        client_socket = -1;
        printf("Waiting for new client connection...\n");
    }
    
    /* Cleanup (should not be reached) */
    close(server_socket);
    return 0;
}