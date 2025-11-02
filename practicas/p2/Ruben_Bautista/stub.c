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
static void* handle_client_connection(void* arg);

// Message queue implementation: circular buffer
static struct message_queue {
    struct message messages[MAX_MESSAGE_QUEUE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
} msg_queue;

// operation_to_string: converts enum operations to string
const char* operation_to_string(enum operations op) {
    switch (op) {
        case READY_TO_SHUTDOWN: return "READY_TO_SHUTDOWN";
        case SHUTDOWN_NOW: return "SHUTDOWN_NOW";
        case SHUTDOWN_ACK: return "SHUTDOWN_ACK";
        default: return "UNKNOWN";
    }
}

// init_message_queue: initializes the message queue
static void init_message_queue() {
    msg_queue.front = 0;
    msg_queue.rear = 0;
    msg_queue.count = 0;
    pthread_mutex_init(&msg_queue.mutex, NULL);
}

// enqueue_message: adds a message to the queue
static void enqueue_message(const struct message* msg) {
    pthread_mutex_lock(&msg_queue.mutex);
    
    if (msg_queue.count < MAX_MESSAGE_QUEUE) {
        msg_queue.messages[msg_queue.rear] = *msg;
        msg_queue.rear = (msg_queue.rear + 1) % MAX_MESSAGE_QUEUE;
        msg_queue.count++;
    }
    
    pthread_mutex_unlock(&msg_queue.mutex);
}

// get_clock_lamport: returns the current Lamport clock value
int get_clock_lamport() {
    int current_clock;
    pthread_mutex_lock(&clock_mutex);
    current_clock = lamport_clock;
    pthread_mutex_unlock(&clock_mutex);
    return current_clock;
}

// increment_clock_for_send: increments the Lamport clock for sending a message
static void increment_clock_for_send() {
    pthread_mutex_lock(&clock_mutex);
    lamport_clock++;
    pthread_mutex_unlock(&clock_mutex);
}

// receiver_thread: thread function to receive messages
static struct client_connections {
    int p1_socket;
    int p3_socket;
    pthread_mutex_t mutex;
} clients = {-1, -1, PTHREAD_MUTEX_INITIALIZER};

static void* receiver_thread(void* arg) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (is_running) {
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            continue;
        }
        
        // Crear thread separado para cada cliente
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_socket;
        
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client_connection, client_sock_ptr) == 0) {
            pthread_detach(client_thread);
        } else {
            free(client_sock_ptr);
            close(client_socket);
        }
    }
    
    return NULL;
}

static void* handle_client_connection(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);
    
    // Guardar información del cliente
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr*)&client_addr, &client_len);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    
    while (is_running) {
        struct message received_msg;
        ssize_t bytes_read = recv(client_socket, &received_msg, sizeof(received_msg), 0);
        
        if (bytes_read == sizeof(received_msg)) {
            /* Guardar/actualizar el socket del cliente */
            pthread_mutex_lock(&clients.mutex);
            if (strcmp(received_msg.origin, "P1") == 0) {
                if (clients.p1_socket != -1) {
                    close(clients.p1_socket); // Cerrar conexión anterior
                }
                clients.p1_socket = client_socket;
                printf("P2: P1 connected from %s:%d\n", client_ip, client_port);
            } else if (strcmp(received_msg.origin, "P3") == 0) {
                if (clients.p3_socket != -1) {
                    close(clients.p3_socket); // Cerrar conexión anterior
                }
                clients.p3_socket = client_socket;
                printf("P2: P3 connected from %s:%d\n", client_ip, client_port);
            }
            pthread_mutex_unlock(&clients.mutex);
            
            pthread_mutex_lock(&clock_mutex);
            if (received_msg.clock_lamport > lamport_clock) {
                lamport_clock = received_msg.clock_lamport;
            }
            lamport_clock++;
            int current_clock = lamport_clock;
            pthread_mutex_unlock(&clock_mutex);
            
            enqueue_message(&received_msg);
            
            printf("%s, %d, RECV (%s), %s\n", 
                   process_name, 
                   current_clock,
                   received_msg.origin,
                   operation_to_string(received_msg.action));
        } else if (bytes_read <= 0) {
            break;
        }
    }
    
    // Cerrar socket cuando el cliente se desconecte
    close(client_socket);
    return NULL;
}

static void* receiver_thread_client(void* arg) {
    while (is_running) {
        struct message received_msg;
        ssize_t bytes_read = recv(server_socket, &received_msg, sizeof(received_msg), 0);
        
        if (bytes_read == sizeof(received_msg)) {
            pthread_mutex_lock(&clock_mutex);
            if (received_msg.clock_lamport > lamport_clock) {
                lamport_clock = received_msg.clock_lamport;
            }
            lamport_clock++;
            int current_clock = lamport_clock;
            pthread_mutex_unlock(&clock_mutex);
            
            enqueue_message(&received_msg);
            
            printf("%s, %d, RECV (%s), %s\n", 
                   process_name, 
                   current_clock,
                   received_msg.origin,
                   operation_to_string(received_msg.action));
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
    
    if (is_p2) {
        /* P2 acts as server - bind and listen */
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
        
        if (listen(server_socket, MAX_CLIENTS) < 0) {
            perror("listen failed");
            close(server_socket);
            return -1;
        }
        
        if (pthread_create(&receiver_thread_id, NULL, receiver_thread, NULL) != 0) {
            perror("thread creation failed");
            close(server_socket);
            return -1;
        }
        
        printf("P2: Server listening on port %d\n", port);
        
    } else {
        /* P1 and P3 are clients - connect to P2 and start receiver thread */
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            perror("socket creation failed");
            return -1;
        }
        
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = inet_addr(ip);
        
        if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect failed");
            close(server_socket);
            return -1;
        }
        
        printf("%s: Connected to P2 at %s:%d\n", proc_name, ip, port);
        
        /* Start receiver thread to listen for messages from P2 */
        if (pthread_create(&receiver_thread_id, NULL, receiver_thread_client, NULL) != 0) {
            perror("thread creation failed");
            close(server_socket);
            return -1;
        }
    }
    return 0;
}

void close_stub() {
    is_running = 0;
    
    if (strcmp(process_name, "P2") == 0) {
        pthread_cancel(receiver_thread_id);
        pthread_join(receiver_thread_id, NULL);
        close(server_socket);
        
        pthread_mutex_lock(&clients.mutex);
        if (clients.p1_socket != -1) close(clients.p1_socket);
        if (clients.p3_socket != -1) close(clients.p3_socket);
        pthread_mutex_unlock(&clients.mutex);
    } else {
        /* P1 y P3 también tienen que cerrar su thread receptor */
        pthread_cancel(receiver_thread_id);
        pthread_join(receiver_thread_id, NULL);
        close(server_socket);
    }
    
    pthread_mutex_destroy(&msg_queue.mutex);
}

int send_message_to_process(const char* target_process, enum operations action) {
    int socket_to_use = -1;
    
    if (strcmp(process_name, "P2") == 0) {
        /* P2 usa las conexiones guardadas de P1 y P3 */
        pthread_mutex_lock(&clients.mutex);
        if (strcmp(target_process, "P1") == 0 && clients.p1_socket != -1) {
            socket_to_use = clients.p1_socket;
        } else if (strcmp(target_process, "P3") == 0 && clients.p3_socket != -1) {
            socket_to_use = clients.p3_socket;
        }
        pthread_mutex_unlock(&clients.mutex);
        
        if (socket_to_use == -1) {
            printf("Error: No connection available for %s\n", target_process);
            return -1;
        }
    } else {
        /* P1 y P3 usan su conexión establecida con P2 */
        socket_to_use = server_socket;
    }
    
    increment_clock_for_send();
    int current_clock = get_clock_lamport();
    
    struct message msg;
    strncpy(msg.origin, process_name, MAX_PROCESS_NAME - 1);
    msg.origin[MAX_PROCESS_NAME - 1] = '\0';
    msg.action = action;
    msg.clock_lamport = current_clock;
    
    ssize_t bytes_sent = send(socket_to_use, &msg, sizeof(msg), MSG_NOSIGNAL);
    
    if (bytes_sent == sizeof(msg)) {
        printf("%s, %d, SEND, %s\n", process_name, current_clock, 
               operation_to_string(action));
        return 0;
    } else {
        printf("Error sending message to %s\n", target_process);
        return -1;
    }
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

/* wait_for_ready_messages(): waits until
 both P1 and P3 send READY_TO_SHUTDOWN messages*/
int wait_for_ready_messages(void) {
    if (strcmp(process_name, "P2") != 0) {
        return -1;
    }
    
    int p1_ready_received = 0;
    int p3_ready_received = 0;
    struct message received_msg;
    
    // wait for both P1 and P3 READY_TO_SHUTDOWN messages
    while (!p1_ready_received || !p3_ready_received) {
        if (has_pending_message()) {
            if (receive_message(&received_msg)) {
                if (strcmp(received_msg.origin, "P1") == 0 && received_msg.action == READY_TO_SHUTDOWN) {
                    if (!p1_ready_received) {
                        p1_ready_received = 1;
                    }
                }
                else if (strcmp(received_msg.origin, "P3") == 0 && received_msg.action == READY_TO_SHUTDOWN) {
                    if (!p1_ready_received) {
                        printf("ERROR: P3 arrived before P1! Please execute P1 first\n");
                        reset_clock();
                        p3_ready_received = 0;
                        // discard all messages from the queue
                        while (has_pending_message()) {
                            receive_message(&received_msg);
                        }
                        continue;
                    } else if (!p3_ready_received) {
                        p3_ready_received = 1;
                    }
                }
            }
        }
        usleep(SLEEP_TIME);
    }
    
    return 1;
}

/* wait_for_messages(): waits until a message with expected
action is received or the lamport clock reaches expected_clock */
int wait_for_message(enum operations expected_action, int expected_clock) {
    struct message received_msg;

    while (1) {
        if (has_pending_message()) {
            if (receive_message(&received_msg)) {
                if (received_msg.action == expected_action) {
                    return 1;
                }
            }
        }
        if (get_clock_lamport() >= expected_clock) {
            return 1;
        }
        usleep(SLEEP_TIME);
    }
    return 0;
}