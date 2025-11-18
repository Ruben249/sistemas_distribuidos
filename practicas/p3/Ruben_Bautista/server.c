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
#include <signal.h>
#include <errno.h>
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

int active_threads_count = 0;
pthread_mutex_t active_threads_mutex;
sem_t available_threads_semaphore;

volatile int server_running = 1;

// parse_server_arguments(): Parses command-line arguments for server configuration.
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

// initialize(): Initializes mutexes, condition variables, and semaphores.
int initialize(void) {
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

// cleanup_resources(): Cleans up ALL resources and forces thread termination
void cleanup_resources(int server_socket) {
    printf("Starting cleanup...\n");

    server_running = 0;

    if (server_socket >= 0) {
        shutdown(server_socket, SHUT_RDWR);
        close(server_socket);
    }

    pthread_mutex_lock(&readers_writers_mutex);
    pthread_cond_broadcast(&readers_can_enter);
    pthread_cond_broadcast(&writers_can_enter);
    pthread_mutex_unlock(&readers_writers_mutex);
    
    for (int i = 0; i < MAX_CONCURRENT_THREADS * 2; i++) {
        sem_post(&available_threads_semaphore);
    }
    
    for (int i = 0; i < 5; i++) {
        pthread_mutex_lock(&active_threads_mutex);
        int remaining = active_threads_count;
        pthread_mutex_unlock(&active_threads_mutex);
        
        if (remaining == 0) break;
        
        printf("Waiting for %d threads...\n", remaining);
        usleep(100000); // 100ms
    }
    
    pthread_mutex_destroy(&counter_mutex);
    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&readers_writers_mutex);
    pthread_mutex_destroy(&active_threads_mutex);
    
    pthread_cond_destroy(&readers_can_enter);
    pthread_cond_destroy(&writers_can_enter);
    
    sem_destroy(&available_threads_semaphore);
}

// write_counter_to_file(): Writes the current counter value to the output file.
void write_counter_to_file(int counter_value) {
    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(OUTPUT_FILENAME, "w");
    if (file != NULL) {
        fprintf(file, "%d", counter_value);
        fclose(file);
    }
    pthread_mutex_unlock(&file_mutex);
}

// get_current_timestamp(): Retrieves the current time in seconds and microseconds.
void get_current_timestamp(long *seconds, long *microseconds) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    *seconds = current_time.tv_sec;
    *microseconds = current_time.tv_usec;
}

// calculate_latency(): Calculates the time difference in nanoseconds.
long calculate_latency(struct timespec start, struct timespec end) {
    long seconds = end.tv_sec - start.tv_sec;
    long nanoseconds = end.tv_nsec - start.tv_nsec;
    return seconds * 1000000000L + nanoseconds;
}

// sleep_random(): Sleeps for a random duration within the critical section.
void sleep_random(void) {
    int sleep_ms = MIN_SLEEP_MS + rand() % (MAX_SLEEP_MS - MIN_SLEEP_MS + 1);
    usleep(sleep_ms * 1000);
}

/* can_pass(): Manages entry into the critical section
 based on reader/writer priority.*/
void can_pass(struct request *client_req, struct timespec *start_time) {
    pthread_mutex_lock(&readers_writers_mutex);
    
    if (client_req->action == READ) {
        if (server_priority == 1) {
            // Priority writers: readers wait if there's an active writer or waiting writers
            while (is_writer_active || waiting_writers_count > 0) {
                waiting_readers_count++;
                pthread_cond_wait(&readers_can_enter, &readers_writers_mutex);
                waiting_readers_count--;
            }
        } else {
            // Priority readers: readers wait only if there's an active writer
            while (is_writer_active) {
                waiting_readers_count++;
                pthread_cond_wait(&readers_can_enter, &readers_writers_mutex);
                waiting_readers_count--;
            }
        }
        active_readers_count++;
    } else {
        waiting_writers_count++;
        
        if (server_priority == 0) {
            // Priority readers: writers wait if there are active or waiting readers
            while (is_writer_active || active_readers_count > 0 || waiting_readers_count > 0) {
                pthread_cond_wait(&writers_can_enter, &readers_writers_mutex);
            }
        } else {
            // Priority writers: writers wait if there is anyone active
            while (is_writer_active || active_readers_count > 0) {
                pthread_cond_wait(&writers_can_enter, &readers_writers_mutex);
            }
        }
        waiting_writers_count--;
        is_writer_active = 1;
    }
    // unlock readers_writers_mutex before returning to allow other threads to proceed
    pthread_mutex_unlock(&readers_writers_mutex);
}

// priority_control(): Lets threads exit the critical section and signals waiting threads.
void priority_control(struct request *client_req) {
    pthread_mutex_lock(&readers_writers_mutex);
    
    if (client_req->action == READ) {
        active_readers_count--;
        
        if (active_readers_count == 0) {
            //server_priority =1 -> priority writers
            if (server_priority == 1) {
                // Priority writers: give priority to writers first
                if (waiting_writers_count > 0) {
                    pthread_cond_signal(&writers_can_enter);
                }
                // Only if there are no writers, give priority to readers
                else if (waiting_readers_count > 0) {
                    pthread_cond_broadcast(&readers_can_enter);
                }
            } else {
                // Priority readers: give priority to readers first
                if (waiting_readers_count > 0) {
                    pthread_cond_broadcast(&readers_can_enter);
                }
                // Only if there are no readers, give priority to writers
                else if (waiting_writers_count > 0) {
                    pthread_cond_signal(&writers_can_enter);
                }
            }
        }
        // server_priority = 0 -> priority readers
    } else {
        is_writer_active = 0;
        
        if (server_priority == 1) {
            // Priority writers: give priority to writers first
            if (waiting_writers_count > 0) {
                pthread_cond_signal(&writers_can_enter);
            }
            // Only if there are no writers, give priority to readers
            else if (waiting_readers_count > 0) {
                pthread_cond_broadcast(&readers_can_enter);
            }
        } else {
            // Priority readers: give priority to readers first
            if (waiting_readers_count > 0) {
                pthread_cond_broadcast(&readers_can_enter);
            }
            // Only if there are no readers, give priority to writers
            else if (waiting_writers_count > 0) {
                pthread_cond_signal(&writers_can_enter);
            }
        }
    }
    
    pthread_mutex_unlock(&readers_writers_mutex);
}

/* manage_request(): Handles the client's request by
reading or writing the shared counter.*/
void manage_request(struct request *req, struct response *resp, long wait_time) {
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
    
    sleep_random();
    pthread_mutex_unlock(&counter_mutex);
    
    resp->action = req->action;
    resp->counter = shared_counter;
    resp->latency_time = wait_time;
}

/* process(): Manages the lifecycle of a client connection,
including receiving requests, processing them, and sending responses. */
void *process(void *client_socket_ptr) {
    int client_socket = *(int *)client_socket_ptr;
    free(client_socket_ptr);
    
    pthread_mutex_lock(&active_threads_mutex);
    active_threads_count++;
    pthread_mutex_unlock(&active_threads_mutex);
    
    struct request client_req;
    struct response client_resp;
    struct timespec start_time, end_time;
    
    if (receive_request(client_socket, &client_req) <= 0) {
        close(client_socket);
        pthread_mutex_lock(&active_threads_mutex);
        active_threads_count--;  // Decrementar si hay error
        pthread_mutex_unlock(&active_threads_mutex);
        sem_post(&available_threads_semaphore);
        return NULL;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    can_pass(&client_req, &start_time);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    long wait_time = calculate_latency(start_time, end_time);
    manage_request(&client_req, &client_resp, wait_time);
    priority_control(&client_req);
    
    send_response(client_socket, &client_resp);
    close(client_socket);
    
    // Decrementar contador al final
    pthread_mutex_lock(&active_threads_mutex);
    active_threads_count--;
    pthread_mutex_unlock(&active_threads_mutex);
    
    sem_post(&available_threads_semaphore);
    
    return NULL;
}

/* manager_thread(): Accepts incoming client connections
 and spawns threads to handle them. */
void *manager_thread(void *server_socket_ptr) {
    int server_socket = *(int *)server_socket_ptr;
    struct timeval timeout;
    fd_set read_fds;
    
    while (server_running) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ready < 0) {
            if (server_running && errno != EINTR) {
                usleep(100000);
            }
            continue;
        }
        
        if (ready == 0) {
            // Timeout - verificar salida
            if (!server_running) break;
            continue;
        }
        
        if (FD_ISSET(server_socket, &read_fds)) {
            int client_socket = accept_client_connection(server_socket);
            if (client_socket < 0) {
                if (server_running && errno != EINTR) {
                    usleep(100000);
                }
                continue;
            }
            
            // Verificar salida inmediatamente después de accept
            if (!server_running) {
                close(client_socket);
                break;
            }
            
            // Esperar semáforo con timeout
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            
            if (sem_timedwait(&available_threads_semaphore, &ts) != 0) {
                close(client_socket);
                if (!server_running) break;
                continue;
            }
            
            // Asignar memoria y verificar
            int *client_socket_ptr = malloc(sizeof(int));
            if (client_socket_ptr == NULL) {
                close(client_socket);
                sem_post(&available_threads_semaphore);
                continue;
            }
            *client_socket_ptr = client_socket;
            
            pthread_t client_thread;
            if (pthread_create(&client_thread, NULL, process, client_socket_ptr) != 0) {
                free(client_socket_ptr);
                close(client_socket);
                sem_post(&available_threads_semaphore);
                continue;
            }
            
            pthread_detach(client_thread);
        }
    }
    
    printf("Acceptor thread finished.\n");
    return NULL;
}
// read_counter_from_file(): Reads the counter value from the output file.
int read_counter_from_file(void) {
    FILE *file = fopen(OUTPUT_FILENAME, "r");
    int counter = 0;
    
    if (file != NULL) {
        if (fscanf(file, "%d", &counter) != 1) {
            counter = 0;
        }
        fclose(file);
    } else {
        file = fopen(OUTPUT_FILENAME, "w");
        if (file != NULL) {
            fprintf(file, "0");
            fclose(file);
        }
    }
    
    return counter;
}

// signal_handler(): Handles SIGINT to gracefully terminate the server.
void signal_handler(int signal) {
    if (signal == SIGINT) {
        server_running = 0;
    }
}

int main(int argc, char *argv[]) {
    int server_port;
    int server_socket;
    pthread_t acceptor_thread;
    
    setbuf(stdout, NULL);
    srand(time(NULL));

    signal(SIGINT, signal_handler);
    
    if (parse_server_arguments(argc, argv, &server_port, &server_priority) != 0) {
        exit(EXIT_FAILURE);
    }
    
    if (initialize() != 0) {
        fprintf(stderr, "Error initializing server resources\n");
        exit(EXIT_FAILURE);
    }
    
    shared_counter = read_counter_from_file();
    
    server_socket = initialize_server_socket(server_port);
    if (server_socket < 0) {
        fprintf(stderr, "Error creating server socket\n");
        cleanup_resources(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d with %s priority\n", 
           server_port, server_priority ? "writers" : "readers");
    
    // Create a thread to accept incoming client connections
    if (pthread_create(&acceptor_thread, NULL, manager_thread, &server_socket) != 0) {
        fprintf(stderr, "Error creating acceptor thread\n");
        close(server_socket);
        cleanup_resources(server_socket);
        exit(EXIT_FAILURE);
    }
    
    pthread_join(acceptor_thread, NULL);
    cleanup_resources(server_socket);
    return 0;
}