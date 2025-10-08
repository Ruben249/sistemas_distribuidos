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
    struct timeval timeout;
    
    setbuf(stdout, NULL);
    
    signal(SIGINT, handle_signal);
    
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket successfully created...\n");
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("connect");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    printf("Connected to the server...\n");
    
    printf("> ");
    fflush(stdout);
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(client_socket, &read_fds);
        
        if (STDIN_FILENO > client_socket) {
            max_fd = STDIN_FILENO;
        } else {
            max_fd = client_socket;
        }
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        
        activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("select");
            break;
        }
        
        if (FD_ISSET(client_socket, &read_fds)) {
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("\n+++ %s\n", buffer);
            } else if (bytes_received == 0) {
                printf("Server disconnected\n");
                break;
            } else {
                printf("> ");
                fflush(stdout);
            }
        }
        
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            printf("> ");
            fflush(stdout);
            
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = 0;
                
                if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
                    printf("Send failed - server disconnected\n");
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