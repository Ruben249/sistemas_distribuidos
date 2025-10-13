#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define MAX_CLIENTS 200
#define BUFFER_SIZE 1024

// Global variables
int server_socket = -1;
volatile sig_atomic_t should_exit = 0;
volatile int active_clients = 0;

// Structure to pass client data to threads
typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_data_t;

/* Handle CTRL+C signal */
void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;
    printf("\nShutting down server...\n");
    
    // Close server socket to stop accepting new connections
    if (server_socket != -1) {
        close(server_socket);
        server_socket = -1;
    }
}

/* Set up and configure server socket */
int setup_server_socket(int port) {
    struct sockaddr_in server_addr;
    int sock_fd;
    const int enable = 1;
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }
    
    // Set SO_REUSEADDR to avoid "Address already in use" errors
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sock_fd);
        return -1;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }
    
    // Listen with backlog = MAX_CLIENTS
    if (listen(sock_fd, MAX_CLIENTS) != 0) {
        perror("listen");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
}

/* Handle communication with a connected client */
void* handle_client_communication(void* arg) {
    client_data_t* client_data = (client_data_t*)arg;
    int client_fd = client_data->client_fd;
    char buffer[BUFFER_SIZE];
    
    int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("+++ %s\n", buffer);
        
        usleep(500000 + (rand() % 1500000));
        
        const char* response = "Hello client!";
        send(client_fd, response, strlen(response), 0);
    }
    
    close(client_fd);
    free(client_data);
    active_clients--;
    
    pthread_exit(NULL);
}
/* Print usage information */
void print_usage(const char* program_name) {
    printf("Usage: %s <port>\n", program_name);
    printf("Example: %s 8000\n", program_name);
}

int main(int argc, char* argv[]) {
    // Disable output buffering for immediate display
    setbuf(stdout, NULL);
    
    // Register signal handler
    signal(SIGINT, handle_signal);
    
    // Validate command line arguments
    if (argc != 2) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize random seed
    srand(time(NULL));
    
    // Set up server socket
    server_socket = setup_server_socket(port);
    if (server_socket == -1) {
        exit(EXIT_FAILURE);
    }
    
    printf("Socket successfully created...\n");
    printf("Socket successfully binded...\n");
    printf("Server listening...\n");
    printf("Maximum concurrent clients: %d\n", MAX_CLIENTS);
    printf("Press Ctrl+C to shutdown the server\n");
    
    // Main server loop
    while (!should_exit) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Use select to make accept non-blocking so we can check should_exit
        fd_set read_fds;
        struct timeval timeout;
        
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && !should_exit) {
            perror("select");
            continue;
        }
        
        if (should_exit) {
            break;
        }
        
        if (activity > 0 && FD_ISSET(server_socket, &read_fds)) {
            // Accept new client connection
            int client_fd = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd == -1) {
                if (!should_exit) {
                    perror("accept");
                }
                continue;
            }
            
            // Check if we can accept more clients
            if (active_clients >= MAX_CLIENTS) {
                printf("Rejecting connection - maximum clients reached (%d)\n", MAX_CLIENTS);
                close(client_fd);
                continue;
            }
            
            // Increment active clients
            active_clients++;
            
            // Create client data structure
            client_data_t* client_data = malloc(sizeof(client_data_t));
            if (!client_data) {
                perror("malloc");
                close(client_fd);
                active_clients--;
                continue;
            }
            
            client_data->client_fd = client_fd;
            memcpy(&client_data->client_addr, &client_addr, sizeof(client_addr));
            
            // Create thread for client
            pthread_t client_thread;
            if (pthread_create(&client_thread, NULL, handle_client_communication, client_data) != 0) {
                perror("pthread_create");
                free(client_data);
                close(client_fd);
                active_clients--;
                continue;
            }
            
            // Detach thread
            pthread_detach(client_thread);
        }
    }
    
    // Wait a bit for active threads to finish
    printf("Waiting for active clients to finish...\n");
    sleep(3); // Wait up to 3 seconds for threads to finish
    
    printf("Server shutdown complete.\n");
    return 0;
}