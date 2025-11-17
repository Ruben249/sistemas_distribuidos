#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <getopt.h>
#include "stub.h"

#define MAX_CONCURRENT_THREADS 600
#define MIN_SLEEP_MS 75
#define MAX_SLEEP_MS 150
#define OUTPUT_FILENAME "server_output.txt"

int shared_counter;
int server_priority;

int active_readers_count;
int waiting_writers_count;
int is_writer_active;
int waiting_readers_count;

pthread_mutex_t counter_mutex;
pthread_mutex_t file_mutex;
pthread_mutex_t readers_writers_mutex;
pthread_cond_t readers_can_enter;
pthread_cond_t writers_can_enter;

int active_threads_count;
pthread_mutex_t active_threads_mutex;
sem_t available_threads_semaphore;

int parse_server_arguments(int argc, char *argv[], int *port, int *priority) {
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"priority", required_argument, 0, 'r'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "p:r:", long_options, &option_index)) != -1) {
        if (opt == 'p') {
            *port = atoi(optarg);
        } else if (opt == 'r') {
            if (strcmp(optarg, "reader") == 0) {
                *priority = 0;
            } else if (strcmp(optarg, "writer") == 0) {
                *priority = 1;
            } else {
                fprintf(stderr, "Error: priority must be reader or writer\n");
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    if (*port == 0) {
        fprintf(stderr, "Usage: %s --port PORT --priority reader/writer\n", argv[0]);
        return -1;
    }
    
    return 0;
}

int initialize_server_resources(void) {
    shared_counter = 0;
    active_readers_count = 0;
    waiting_writers_count = 0;
    waiting_readers_count = 0;
    is_writer_active = 0;
    active_threads_count = 0;
    
    if (pthread_mutex_init(&counter_mutex, NULL) != 0) return -1;
    if (pthread_mutex_init(&file_mutex, NULL) != 0) return -1;
    if (pthread_mutex_init(&readers_writers_mutex, NULL) != 0) return -1;
    if (pthread_mutex_init(&active_threads_mutex, NULL) != 0) return -1;
    if (pthread_cond_init(&readers_can_enter, NULL) != 0) return -1;
    if (pthread_cond_init(&writers_can_enter, NULL) != 0) return -1;
    if (sem_init(&available_threads_semaphore, 0, MAX_CONCURRENT_THREADS) != 0) return -1;
    
    return 0;
}

void cleanup_server_resources(void) {
    pthread_mutex_destroy(&counter_mutex);
    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&readers_writers_mutex);
    pthread_mutex_destroy(&active_threads_mutex);
    pthread_cond_destroy(&readers_can_enter);
    pthread_cond_destroy(&writers_can_enter);
    sem_destroy(&available_threads_semaphore);
}

int read_counter_from_file(void) {
    FILE *file = fopen(OUTPUT_FILENAME, "r");
    int counter = 0;
    
    if (file != NULL) {
        fscanf(file, "%d", &counter);
        fclose(file);
    }
    
    return counter;
}

void write_counter_to_file(int counter_value) {
    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(OUTPUT_FILENAME, "w");
    if (file != NULL) {
        fprintf(file, "%d", counter_value);
        fclose(file);
    }
    pthread_mutex_unlock(&file_mutex);
}

void get_current_timestamp(long *seconds, long *microseconds) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    *seconds = current_time.tv_sec;
    *microseconds = current_time.tv_usec;
}

long calculate_time_difference_ns(struct timespec start, struct timespec end) {
    long seconds = end.tv_sec - start.tv_sec;
    long nanoseconds = end.tv_nsec - start.tv_nsec;
    return seconds * 1000000000L + nanoseconds;
}

void random_sleep_in_critical_section(void) {
    int sleep_ms = MIN_SLEEP_MS + rand() % (MAX_SLEEP_MS - MIN_SLEEP_MS + 1);
    usleep(sleep_ms * 1000);
}

void enter_critical_section(struct request *client_req, struct timespec *start_time) {
    pthread_mutex_lock(&readers_writers_mutex);
    
    if (client_req->action == READ) {
        if (server_priority == 1) {
            waiting_readers_count++;
            while (is_writer_active || waiting_writers_count > 0) {
                pthread_cond_wait(&readers_can_enter, &readers_writers_mutex);
            }
            waiting_readers_count--;
        } else {
            while (is_writer_active) {
                pthread_cond_wait(&readers_can_enter, &readers_writers_mutex);
            }
        }
        active_readers_count++;
    } else {
        waiting_writers_count++;
        while (is_writer_active || active_readers_count > 0) {
            pthread_cond_wait(&writers_can_enter, &readers_writers_mutex);
        }
        waiting_writers_count--;
        is_writer_active = 1;
    }
    
    pthread_mutex_unlock(&readers_writers_mutex);
}

void exit_critical_section(struct request *client_req) {
    pthread_mutex_lock(&readers_writers_mutex);
    
    if (client_req->action == READ) {
        active_readers_count--;
        if (active_readers_count == 0 && waiting_writers_count > 0) {
            pthread_cond_signal(&writers_can_enter);
        }
    } else {
        is_writer_active = 0;
        if (server_priority == 1) {
            if (waiting_writers_count > 0) {
                pthread_cond_signal(&writers_can_enter);
            } else if (waiting_readers_count > 0) {
                pthread_cond_broadcast(&readers_can_enter);
            }
        } else {
            if (waiting_writers_count > 0) {
                pthread_cond_signal(&writers_can_enter);
            } else {
                pthread_cond_broadcast(&readers_can_enter);
            }
        }
    }
    
    pthread_mutex_unlock(&readers_writers_mutex);
}

void process_client_request(struct request *req, struct response *resp, long wait_time) {
    long seconds, microseconds;
    get_current_timestamp(&seconds, &microseconds);
    
    pthread_mutex_lock(&counter_mutex);
    
    if (req->action == WRITE) {
        shared_counter++;
        printf("[%ld.%06ld][ESCRITOR #%d] modifica contador con valor %d\n", 
               seconds, microseconds, req->id, shared_counter);
        write_counter_to_file(shared_counter);
    } else {
        printf("[%ld.%06ld][LECTOR #%d] lee contador con valor %d\n", 
               seconds, microseconds, req->id, shared_counter);
    }
    
    random_sleep_in_critical_section();
    pthread_mutex_unlock(&counter_mutex);
    
    resp->action = req->action;
    resp->counter = shared_counter;
    resp->latency_time = wait_time;
}

void *handle_client_thread(void *client_socket_ptr) {
    int client_socket = *(int *)client_socket_ptr;
    free(client_socket_ptr);
    
    struct request client_req;
    struct response client_resp;
    struct timespec start_time, end_time;
    int bytes_received, bytes_sent;
    int total_sent = 0;
    
    bytes_received = recv(client_socket, &client_req, sizeof(struct request), 0);
    if (bytes_received <= 0) {
        close_client_connection(client_socket);
        goto cleanup;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    enter_critical_section(&client_req, &start_time);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    long wait_time = calculate_time_difference_ns(start_time, end_time);
    process_client_request(&client_req, &client_resp, wait_time);
    exit_critical_section(&client_req);
    
    // Robust send to ensure complete response delivery
    char *response_ptr = (char *)&client_resp;
    int remaining_bytes = sizeof(struct response);
    
    while (remaining_bytes > 0) {
        bytes_sent = send(client_socket, response_ptr, remaining_bytes, 0);
        if (bytes_sent <= 0) {
            break;
        }
        response_ptr += bytes_sent;
        remaining_bytes -= bytes_sent;
        total_sent += bytes_sent;
    }
    
    close_client_connection(client_socket);

cleanup:
    pthread_mutex_lock(&active_threads_mutex);
    active_threads_count--;
    pthread_mutex_unlock(&active_threads_mutex);
    sem_post(&available_threads_semaphore);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    int server_port;
    int server_socket;
    
    setbuf(stdout, NULL);
    srand(time(NULL));
    
    if (parse_server_arguments(argc, argv, &server_port, &server_priority) != 0) {
        exit(EXIT_FAILURE);
    }
    
    if (initialize_server_resources() != 0) {
        fprintf(stderr, "Error initializing server resources\n");
        exit(EXIT_FAILURE);
    }
    
    shared_counter = read_counter_from_file();
    
    server_socket = initialize_server_socket(server_port);
    if (server_socket < 0) {
        fprintf(stderr, "Error creating server socket\n");
        cleanup_server_resources();
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        sem_wait(&available_threads_semaphore);
        
        int client_socket = accept_client_connection(server_socket);
        if (client_socket < 0) {
            sem_post(&available_threads_semaphore);
            continue;
        }
        
        pthread_mutex_lock(&active_threads_mutex);
        active_threads_count++;
        pthread_mutex_unlock(&active_threads_mutex);
        
        int *client_socket_ptr = malloc(sizeof(int));
        *client_socket_ptr = client_socket;
        
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client_thread, client_socket_ptr) != 0) {
            free(client_socket_ptr);
            close_client_connection(client_socket);
            pthread_mutex_lock(&active_threads_mutex);
            active_threads_count--;
            pthread_mutex_unlock(&active_threads_mutex);
            sem_post(&available_threads_semaphore);
            continue;
        }
        
        pthread_detach(client_thread);
    }
    
    close_server_socket(server_socket);
    cleanup_server_resources();
    return 0;
}