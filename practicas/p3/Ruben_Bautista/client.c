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

void *client_thread_function(void *thread_id_ptr) {
    int thread_id = *(int *)thread_id_ptr;
    int client_socket;
    struct request client_req;
    struct response server_resp;
    
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
    
    if (send(client_socket, &client_req, sizeof(struct request), 0) <= 0) {
        fprintf(stderr, "[Cliente #%d] Error sending request\n", thread_id);
        close_client_connection(client_socket);
        return NULL;
    }
    
    if (recv(client_socket, &server_resp, sizeof(struct response), 0) <= 0) {
        fprintf(stderr, "[Cliente #%d] Error receiving response\n", thread_id);
        close_client_connection(client_socket);
        return NULL;
    }
    
    print_thread_result(thread_id, &server_resp);
    close_client_connection(client_socket);
    
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
    
    threads = malloc(number_of_threads * sizeof(pthread_t));
    thread_ids = malloc(number_of_threads * sizeof(int));
    if (threads == NULL || thread_ids == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    for (i = 0; i < number_of_threads; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, client_thread_function, &thread_ids[i]) != 0) {
            fprintf(stderr, "Error: Thread creation failed\n");
            exit(EXIT_FAILURE);
        }
    }
    
    for (i = 0; i < number_of_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    free(thread_ids);
    return 0;
}