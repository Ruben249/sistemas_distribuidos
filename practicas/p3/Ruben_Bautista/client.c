#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <getopt.h>
#include "stub.h"

char *server_ip_address = NULL;
int server_port_number = 0;
int client_mode = 0;
int number_of_threads = 0;

//print_thread_result(): Prints the result of a thread's operation.
void print_thread_result(int thread_id, struct response *resp) {
    const char *mode_str;
    if (resp->action == READ) {
        mode_str = "Lector";
    } else {
        mode_str = "Escritor";
    }
    printf("[Cliente #%d] %s, contador=%d, tiempo=%ld ns\n", 
           thread_id, mode_str, resp->counter, resp->latency_time);
}

// parse_client_arguments(): Parses command-line arguments for client configuration.
int parse_client_arguments(int argc, char *argv[], char **ip, int *port, int *mode, int *threads) {
    static struct option long_options[] = {
        {"ip", required_argument, 0, 'i'},
        {"port", required_argument, 0, 'p'},
        {"mode", required_argument, 0, 'm'},
        {"threads", required_argument, 0, 't'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "i:p:m:t:", long_options, &option_index)) != -1) {
        if (opt == 'i') {
            *ip = optarg;
        } else if (opt == 'p') {
            *port = atoi(optarg);
        } else if (opt == 'm') {
            if (strcmp(optarg, "reader") == 0) {
                *mode = 0;
            } else if (strcmp(optarg, "writer") == 0) {
                *mode = 1;
            } else {
                fprintf(stderr, "Error: mode must be reader or writer\n");
                return -1;
            }
        } else if (opt == 't') {
            *threads = atoi(optarg);
            if (*threads <= 0) {
                fprintf(stderr, "Error: threads must be positive integer\n");
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    if (*ip == NULL || *port == 0 || *threads == 0) {
        fprintf(stderr, "Usage: %s --ip IP --port PORT --mode reader/writer --threads N\n", argv[0]);
        return -1;
    }
    
    return 0;
}

// comunication_server(): sends requests to the server and processes responses.
void *comunication_server(void *thread_id_ptr) {
    int thread_id = *(int *)thread_id_ptr;
    int client_socket;
    struct request client_req;
    struct response server_resp;
    int bytes_sent, bytes_received;
    int total_sent = 0, total_received = 0;
    
    client_socket = connect_to_server(server_ip_address, server_port_number);
    if (client_socket < 0) {
        fprintf(stderr, "[Cliente #%d] Error connecting to server\n", thread_id);
        return NULL;
    }
    
    if (client_mode == 0) {
        client_req.action = READ;
    } else {
        client_req.action = WRITE;
    }
    client_req.id = thread_id;
    
    // Send the request to server
    char *request_ptr = (char *)&client_req;
    int remaining_request_bytes = sizeof(struct request);
    
    while (remaining_request_bytes > 0) {
        bytes_sent = send(client_socket, request_ptr, remaining_request_bytes, MSG_NOSIGNAL);
        if (bytes_sent <= 0) {
            fprintf(stderr, "[Cliente #%d] Error sending request\n", thread_id);
            close(client_socket);
            return NULL;
        }
        request_ptr += bytes_sent;
        remaining_request_bytes -= bytes_sent;
        total_sent += bytes_sent;
    }
    
    // Reception from server
    char *response_ptr = (char *)&server_resp;
    int remaining_response_bytes = sizeof(struct response);
    
    while (remaining_response_bytes > 0) {
        bytes_received = recv(client_socket, response_ptr, remaining_response_bytes, 0);
        if (bytes_received <= 0) {
            fprintf(stderr, "[Cliente #%d] Error receiving response\n", thread_id);
            close(client_socket);
            return NULL;
        }
        response_ptr += bytes_received;
        remaining_response_bytes -= bytes_received;
        total_received += bytes_received;
    }
    
    print_thread_result(thread_id, &server_resp);
    close(client_socket);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t *threads;
    int *thread_ids;
    int i;
    
    setbuf(stdout, NULL);
    
    if (parse_client_arguments(argc, argv, &server_ip_address, &server_port_number, 
                              &client_mode, &number_of_threads) != 0) {
        exit(EXIT_FAILURE);
    }
    
    // Save memory for threads and their IDs
    threads = malloc(number_of_threads * sizeof(pthread_t));
    thread_ids = malloc(number_of_threads * sizeof(int));
    if (threads == NULL || thread_ids == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    // Create client threads
    for (i = 0; i < number_of_threads; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, comunication_server, &thread_ids[i]) != 0) {
            fprintf(stderr, "Error: Thread creation failed\n");
            exit(EXIT_FAILURE);
        }
    }
    
    // Wait for all threads to finish
    for (i = 0; i < number_of_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    free(thread_ids);
    return 0;
}