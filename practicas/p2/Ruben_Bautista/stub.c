#include "stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

static unsigned int lamport_clock = 0;
static pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;

static char process_name[MAX_PROCESS_NAME];
static int server_port;
static int server_socket;
static pthread_t receiver_thread_id;

static struct message_queue {
    struct message messages[MAX_MESSAGE_QUEUE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
} msg_queue;

static int is_running = 1;

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

void increment_clock() {
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

static void* receiver_thread(void* arg) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (is_running) {
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, 
                                  &client_len);
        if (client_socket < 0) {
            usleep(50000);
            continue;
        }
        
        struct message received_msg;
        ssize_t bytes_read = recv(client_socket, &received_msg, sizeof(received_msg), 0);
        
        if (bytes_read == sizeof(received_msg)) {
            update_clock_on_receive(received_msg.clock_lamport);
            enqueue_message(&received_msg);
            
            printf("%s, %d, RECV (%s), %s\n", process_name, 
                   received_msg.clock_lamport, received_msg.origin,
                   operation_to_string(received_msg.action));
        }
        
        close(client_socket);
    }
    
    return NULL;
}

int init_stub(const char* proc_name, const char* ip, int port) {
    strncpy(process_name, proc_name, MAX_PROCESS_NAME - 1);
    process_name[MAX_PROCESS_NAME - 1] = '\0';
    server_port = port;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_socket);
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_socket);
        return -1;
    }
    
    if (listen(server_socket, MAX_MESSAGE_QUEUE) < 0) {
        perror("listen failed");
        close(server_socket);
        return -1;
    }
    
    init_message_queue();
    
    if (pthread_create(&receiver_thread_id, NULL, receiver_thread, NULL) != 0) {
        perror("thread creation failed");
        close(server_socket);
        return -1;
    }
    
    printf("%s: Stub initialized on port %d\n", process_name, port);
    return 0;
}

void close_stub() {
    is_running = 0;
    pthread_cancel(receiver_thread_id);
    pthread_join(receiver_thread_id, NULL);
    close(server_socket);
    pthread_mutex_destroy(&msg_queue.mutex);
}

int send_message(const char* dest_ip, int dest_port, enum operations action) {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_port);
    
    if (inet_pton(AF_INET, dest_ip, &server_addr.sin_addr) <= 0) {
        close(client_socket);
        return -1;
    }
    
    // Retry connection logic for distributed environment
    int connected = 0;
    for (int attempt = 0; attempt < CONNECTION_RETRIES && !connected; attempt++) {
        if (connect(client_socket, (struct sockaddr*)&server_addr, 
                   sizeof(server_addr)) == 0) {
            connected = 1;
        } else {
            usleep(RETRY_DELAY);
        }
    }
    
    if (!connected) {
        close(client_socket);
        return -1;
    }
    
    increment_clock();
    int current_clock = get_clock_lamport();
    
    struct message msg;
    strncpy(msg.origin, process_name, MAX_PROCESS_NAME - 1);
    msg.origin[MAX_PROCESS_NAME - 1] = '\0';
    msg.action = action;
    msg.clock_lamport = current_clock;
    
    ssize_t bytes_sent = send(client_socket, &msg, sizeof(msg), 0);
    close(client_socket);
    
    if (bytes_sent == sizeof(msg)) {
        printf("%s, %d, SEND, %s\n", process_name, current_clock, 
               operation_to_string(action));
        return 0;
    }
    
    return -1;
}

int has_pending_message() {
    pthread_mutex_lock(&msg_queue.mutex);
    int result = (msg_queue.count > 0);
    pthread_mutex_unlock(&msg_queue.mutex);
    return result;
}

void process_pending_messages() {
    // Messages are automatically processed by receiver thread
    // This function exists for API consistency
    usleep(SLEEP_INTERVAL);
}

const char* operation_to_string(enum operations op) {
    switch (op) {
        case READY_TO_SHUTDOWN: return "READY_TO_SHUTDOWN";
        case SHUTDOWN_NOW: return "SHUTDOWN_NOW";
        case SHUTDOWN_ACK: return "SHUTDOWN_ACK";
        default: return "UNKNOWN";
    }
}