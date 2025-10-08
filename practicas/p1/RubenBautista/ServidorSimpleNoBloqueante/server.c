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

int server_socket = -1;
int connection_socket = -1;
int should_exit = 0;

void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;
}

int setup_server_socket(void) {
    struct sockaddr_in server_addr;
    int sock_fd;
    const int enable = 1;
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt");
        close(sock_fd);
        return -1;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }
    
    if (listen(sock_fd, MAX_CLIENTS) != 0) {
        perror("listen");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
}

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

void handle_client_communication(int client_fd) {
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_fd, activity;
    int client_connected = 1;
    struct timeval timeout;
    
    printf("> ");
    fflush(stdout);
    
    while (client_connected && should_exit == 0) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(client_fd, &read_fds);
        
        if (STDIN_FILENO > client_fd) {
            max_fd = STDIN_FILENO;
        } else {
            max_fd = client_fd;
        }
        
        timeout.tv_sec = 1;  /* 1 segundo de timeout para servidor */
        timeout.tv_usec = 0;
        
        activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("select");
            break;
        }
        
        if (activity == 0) {
            /* Timeout - no tiene nada que leer del socket */
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        if (FD_ISSET(client_fd, &read_fds)) {
            int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("\n+++ %s\n", buffer);
            } else if (bytes_received == 0) {
                printf("Client disconnected\n");
                client_connected = 0;
                break;
            }
        }
        
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
    setbuf(stdout, NULL);
    
    signal(SIGINT, handle_signal);
    
    while (should_exit == 0) {
        server_socket = setup_server_socket();
        if (server_socket == -1) {
            exit(EXIT_FAILURE);
        }
        printf("Socket successfully created...\n");
        printf("Socket successfully binded...\n");
        printf("Server listening...\n");

        connection_socket = accept_client_connection(server_socket);
        if (connection_socket == -1) {
            close(server_socket);
            continue;
        }
        
        close(server_socket);
        server_socket = -1;
        
        handle_client_communication(connection_socket);
        
        close(connection_socket);
        connection_socket = -1;
        
        printf("Waiting for new client connection...\n");
    }
    
    if (connection_socket != -1) {
        close(connection_socket);
    }
    if (server_socket != -1) {
        close(server_socket);
    }
    printf("\nShutting down server...\n");
    return 0;
}