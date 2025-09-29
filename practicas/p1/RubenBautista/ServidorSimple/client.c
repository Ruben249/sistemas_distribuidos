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

#ifdef DEBUG
    #define DEBUG_PRINTF(...) printf("DEBUG: " __VA_ARGS__)
#else
    #define DEBUG_PRINTF(...)
#endif

/* Global variable for signal handling */
int client_socket = -1;

/* Function prototypes */
void handle_signal(int sig);
int setup_client_socket(void);
void handle_server_communication(int sock_fd);
void print_prompt(void);
void print_received_message(const char* message);

/* Signal handler for graceful shutdown */
void handle_signal(int sig) {
    (void)sig;  /* Avoid unused parameter warning */
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
    
    /* Create client socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation failed");
        return -1;
    }
    
    /* Configure server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock_fd);
        return -1;
    }
    
    /* Connect to server */
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("Connection to server failed");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
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

/* Handle communication with server */
void handle_server_communication(int sock_fd) {
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_fd, activity;
    
    print_prompt();
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sock_fd, &read_fds);
        
        max_fd = (STDIN_FILENO > sock_fd) ? STDIN_FILENO : sock_fd;
        
        /* Wait for activity on either input or socket */
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (activity < 0) {
            perror("Select error");
            break;
        }
        
        /* Handle incoming message from server */
        if (FD_ISSET(sock_fd, &read_fds)) {
            int bytes_received = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_received <= 0) {
                printf("Server disconnected\n");
                break;
            }
            
            buffer[bytes_received] = '\0';
            print_received_message(buffer);
            DEBUG_PRINTF("Received from server: %s\n", buffer);
            print_prompt();
        }
        
        /* Handle user input */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = 0;  /* Remove newline */
                
                if (send(sock_fd, buffer, strlen(buffer), 0) == -1) {
                    perror("Send failed");
                    break;
                }
                DEBUG_PRINTF("Sent to server: %s\n", buffer);
                print_prompt();
            }
        }
    }
}

int main() {
    /* Set up signal handling */
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