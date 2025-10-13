#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

// Structure for message exchange (optional)
typedef struct {
    char message[BUFFER_SIZE];
} message_t;

// Global variable for signal handling
volatile sig_atomic_t client_should_exit = 0;

/* Handle CTRL+C signal */
void handle_signal(int sig) {
    (void)sig;
    client_should_exit = 1;
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int client_socket = -1;
    
    setbuf(stdout, NULL);
    
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client_id> <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int client_id = atoi(argv[1]);
    char* server_ip = argv[2];
    int server_port = atoi(argv[3]);
    
    if (client_id <= 0) {
        fprintf(stderr, "Error: Client ID must be a positive number\n");
        exit(EXIT_FAILURE);
    }
    
    if (server_port <= 0 || server_port > 65535) {
        fprintf(stderr, "Error: Invalid server port\n");
        exit(EXIT_FAILURE);
    }
    
    signal(SIGINT, handle_signal);
    
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("connect");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    snprintf(buffer, BUFFER_SIZE, "Hello server! From client: %d", client_id);
    
    if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
        perror("send");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("+++ %s\n", buffer);  // AÑADIDO: Mostrar "Hello client!"
    } else if (bytes_received < 0) {
        perror("recv");
    }
    
    close(client_socket);
    return 0;
}