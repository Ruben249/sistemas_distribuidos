#include "stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static unsigned int lamport_clock = 0;
static int is_running = 1;
static pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;

static char process_name[MAX_PROCESS_NAME];
static int server_socket;
static int server_port;
static pthread_t receiver_thread_id;

static struct client_connections {
    int p1_socket;
    int p3_socket;
    pthread_mutex_t mutex;
} clients = {-1, -1, PTHREAD_MUTEX_INITIALIZER};

static struct message_queue {
    struct message messages[MAX_MESSAGE_QUEUE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
} msg_queue;

static void init_message_queue() {
    msg_queue.front = 0;
    msg_queue.rear = 0;
    msg_queue.count = 0;
    pthread_mutex_init(&msg_queue.mutex, NULL);
}

static void enqueue_message(const struct message* msg) {
    pthread_mutex_lock(&msg_queue.mutex);
    if (msg_queue.count < MAX_MESSAGE_QUEUE) {
        msg_queue.messages[msg_queue.rear] = *msg;
        msg_queue.rear = (msg_queue.rear + 1) % MAX_MESSAGE_QUEUE;
        msg_queue.count++;
    }
    pthread_mutex_unlock(&msg_queue.mutex);
}

int get_clock_lamport() {
    int current_clock;
    pthread_mutex_lock(&clock_mutex);
    current_clock = lamport_clock;
    pthread_mutex_unlock(&clock_mutex);
    return current_clock;
}

static void increment_clock() {
    pthread_mutex_lock(&clock_mutex);
    lamport_clock++;
    pthread_mutex_unlock(&clock_mutex);
}

static void update_clock_on_receive(int received_clock) {
    pthread_mutex_lock(&clock_mutex);
    if (received_clock > lamport_clock) {
        lamport_clock = received_clock;
    }
    lamport_clock++;
    pthread_mutex_unlock(&clock_mutex);
}

static void process_received_message(const struct message* msg) {
    update_clock_on_receive(msg->clock_lamport);
    enqueue_message(msg);
    
    printf("%s, %d, RECV (%s), ", process_name, msg->clock_lamport, msg->origin);
    switch (msg->action) {
        case READY_TO_SHUTDOWN: printf("READY_TO_SHUTDOWN\n"); break;
        case SHUTDOWN_NOW: printf("SHUTDOWN_NOW\n"); break;
        case SHUTDOWN_ACK: printf("SHUTDOWN_ACK\n"); break;
    }
}

static void* handle_client_connection(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);
    
    while (is_running) {
        struct message received_msg;
        ssize_t bytes_read = recv(client_socket, &received_msg, sizeof(received_msg), 0);
        
        if (bytes_read == sizeof(received_msg)) {
            pthread_mutex_lock(&clients.mutex);
            if (strcmp(received_msg.origin, "P1") == 0) {
                if (clients.p1_socket != -1) close(clients.p1_socket);
                clients.p1_socket = client_socket;
            } else if (strcmp(received_msg.origin, "P3") == 0) {
                if (clients.p3_socket != -1) close(clients.p3_socket);
                clients.p3_socket = client_socket;
            }
            pthread_mutex_unlock(&clients.mutex);
            
            process_received_message(&received_msg);
        } else if (bytes_read <= 0) {
            break;
        }
    }
    
    close(client_socket);
    return NULL;
}

static void* receiver_thread_server(void* arg) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (is_running) {
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) continue;
        
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_socket;
        
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client_connection, client_sock_ptr) != 0) {
            free(client_sock_ptr);
            close(client_socket);
        } else {
            pthread_detach(client_thread);
        }
    }
    return NULL;
}

static void* receiver_thread_client(void* arg) {
    while (is_running) {
        struct message received_msg;
        ssize_t bytes_read = recv(server_socket, &received_msg, sizeof(received_msg), 0);
        if (bytes_read == sizeof(received_msg)) {
            process_received_message(&received_msg);
        } else if (bytes_read <= 0) {
            break;
        }
    }
    return NULL;
}

int init_stub(const char* proc_name, const char* ip, int port) {
    strncpy(process_name, proc_name, MAX_PROCESS_NAME - 1);
    process_name[MAX_PROCESS_NAME - 1] = '\0';
    server_port = port;
    
    int is_p2 = (strcmp(proc_name, "P2") == 0);
    init_message_queue();
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (is_p2) {
        server_addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 ||
            listen(server_socket, MAX_CLIENTS) < 0) {
            close(server_socket);
            return -1;
        }
        pthread_create(&receiver_thread_id, NULL, receiver_thread_server, NULL);
    } else {
        server_addr.sin_addr.s_addr = inet_addr(ip);
        if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(server_socket);
            return -1;
        }
        pthread_create(&receiver_thread_id, NULL, receiver_thread_client, NULL);
    }
    
    return 0;
}

void close_stub() {
    is_running = 0;
    pthread_cancel(receiver_thread_id);
    pthread_join(receiver_thread_id, NULL);
    close(server_socket);
    
    if (strcmp(process_name, "P2") == 0) {
        pthread_mutex_lock(&clients.mutex);
        if (clients.p1_socket != -1) close(clients.p1_socket);
        if (clients.p3_socket != -1) close(clients.p3_socket);
        pthread_mutex_unlock(&clients.mutex);
    }
    
    pthread_mutex_destroy(&msg_queue.mutex);
}

int send_message_to_process(const char* target_process, enum operations action) {
    int target_socket = server_socket;
    
    if (strcmp(process_name, "P2") == 0) {
        pthread_mutex_lock(&clients.mutex);
        if (strcmp(target_process, "P1") == 0) target_socket = clients.p1_socket;
        else if (strcmp(target_process, "P3") == 0) target_socket = clients.p3_socket;
        pthread_mutex_unlock(&clients.mutex);
        
        if (target_socket == -1) return -1;
    }
    
    increment_clock();
    int current_clock = get_clock_lamport();
    
    struct message msg;
    memset(&msg, 0, sizeof(msg)); // Inicializar toda la estructura a 0
    strncpy(msg.origin, process_name, MAX_PROCESS_NAME - 1);
    msg.origin[MAX_PROCESS_NAME - 1] = '\0'; // Asegurar terminación nula
    msg.action = action;
    msg.clock_lamport = current_clock;
    
    if (send(target_socket, &msg, sizeof(msg), MSG_NOSIGNAL) == sizeof(msg)) {
        printf("%s, %d, SEND, ", process_name, current_clock);
        switch (action) {
            case READY_TO_SHUTDOWN: printf("READY_TO_SHUTDOWN\n"); break;
            case SHUTDOWN_NOW: printf("SHUTDOWN_NOW\n"); break;
            case SHUTDOWN_ACK: printf("SHUTDOWN_ACK\n"); break;
        }
        return 0;
    }
    return -1;
}

int has_pending_message(void) {
    pthread_mutex_lock(&msg_queue.mutex);
    int result = (msg_queue.count > 0);
    pthread_mutex_unlock(&msg_queue.mutex);
    return result;
}

int receive_message(struct message* msg) {
    pthread_mutex_lock(&msg_queue.mutex);
    if (msg_queue.count > 0) {
        *msg = msg_queue.messages[msg_queue.front];
        msg_queue.front = (msg_queue.front + 1) % MAX_MESSAGE_QUEUE;
        msg_queue.count--;
        pthread_mutex_unlock(&msg_queue.mutex);
        return 1;
    }
    pthread_mutex_unlock(&msg_queue.mutex);
    return 0;
}

void reset_clock(void) {
    pthread_mutex_lock(&clock_mutex);
    lamport_clock = 0;
    pthread_mutex_unlock(&clock_mutex);
}

int wait_for_ready_messages(void) {
    if (strcmp(process_name, "P2") != 0) return -1;
    
    int p1_ready = 0, p3_ready = 0;
    struct message received_msg;
    
    while (!p1_ready || !p3_ready) {
        if (has_pending_message() && receive_message(&received_msg)) {
            if (strcmp(received_msg.origin, "P1") == 0 && received_msg.action == READY_TO_SHUTDOWN) {
                p1_ready = 1;
            } else if (strcmp(received_msg.origin, "P3") == 0 && received_msg.action == READY_TO_SHUTDOWN) {
                if (!p1_ready) {
                    printf("ERROR: P3 arrived before P1! Please execute P1 first\n");
                    reset_clock();
                    while (has_pending_message()) receive_message(&received_msg);
                } else {
                    p3_ready = 1;
                }
            }
        }
        usleep(SLEEP_TIME);
    }
    return 1;
}