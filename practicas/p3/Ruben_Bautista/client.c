#include "stub.h"

char *server_ip_address = NULL;
int server_port_number = 0;
int client_mode = 0;
int number_of_threads = 0;

volatile sig_atomic_t interrupted = 0;
pthread_t *threads = NULL;
int *thread_ids = NULL;
int number_of_created_threads = 0;

// print_thread_result(): Prints the result of a thread's operation.
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

// parse_client_arguments(): Parses command-line arguments for the client.
int parse_client_arguments(int argc, char *argv[], char **ip, int *port, int *mode, int *threadss) {
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
            *threadss = atoi(optarg);
            if (*threadss <= 0) {
                fprintf(stderr, "Error: threads must be positive integer\n");
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    if (*ip == NULL || *port == 0 || *threadss == 0) {
        fprintf(stderr, "Usage: %s --ip IP --port PORT --mode reader/writer --threads N\n", argv[0]);
        return -1;
    }
    
    return 0;
}

// comunication_server(): Thread function to communicate with the server.
void *comunication_server(void *thread_id_ptr) {
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
    
    if (send_request(client_socket, &client_req) <= 0) {
        fprintf(stderr, "[Cliente #%d] Error sending request\n", thread_id);
        close_connection(client_socket);
        return NULL;
    }
    
    if (receive_response(client_socket, &server_resp) <= 0) {
        fprintf(stderr, "[Cliente #%d] Error receiving response\n", thread_id);
        close_connection(client_socket);
        return NULL;
    }
    
    print_thread_result(thread_id, &server_resp);
    close_connection(client_socket);
    
    return NULL;
}

// handle_signal(): Signal handler for SIGINT (Ctrl+C).
void handle_signal(int sig) {
    interrupted = 1;
}

// cleanup_resources(): Frees allocated resources and waits for threads to finish.
void cleanup_resources() {
    printf("[Cliente] Esperando a que terminen los hilos en ejecución...\n");
    
    if (threads != NULL) {
        for (int i = 0; i < number_of_created_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        free(threads);
        threads = NULL;
    }
    if (thread_ids != NULL) {
        free(thread_ids);
        thread_ids = NULL;
    }
}

int main(int argc, char *argv[]) {
    int i;
    
    setbuf(stdout, NULL);

    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    
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
    
    number_of_created_threads = 0;
    
    for (i = 0; i < number_of_threads && !interrupted; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, comunication_server, &thread_ids[i]) != 0) {
            fprintf(stderr, "Error: Thread creation failed\n");
            cleanup_resources();
            exit(EXIT_FAILURE);
        }
        number_of_created_threads++;
    }
    
    cleanup_resources();
    
    return 0;
}