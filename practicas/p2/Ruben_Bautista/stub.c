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
static void* receiver_thread(void* arg) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // While the server is running, accept incoming connections
    while (is_running) {
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            continue;
        }
        
        // Get client IP and port
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        
        struct message received_msg;
        ssize_t bytes_read = recv(client_socket, &received_msg, sizeof(received_msg), 0);
        
        if (bytes_read == sizeof(received_msg)) {
            /* Verificar que no sea un mensaje de nosotros mismos */
            if (strcmp(received_msg.origin, process_name) != 0) {
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
            }
        }
        
        close(client_socket);
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
        /* P1 and P3 also need to receive messages - create their own server sockets */
        int client_server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_server_socket < 0) {
            perror("socket creation failed");
            return -1;
        }
        
        int opt = 1;
        if (setsockopt(client_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt failed");
            close(client_server_socket);
            return -1;
        }
        
        struct sockaddr_in client_server_addr;
        client_server_addr.sin_family = AF_INET;
        client_server_addr.sin_addr.s_addr = INADDR_ANY;
        
        /* P1 uses port+1, P3 uses port+2 to avoid conflicts */
        int client_port = port;
        if (strcmp(proc_name, "P1") == 0) {
            client_port = port + 1;
        } else if (strcmp(proc_name, "P3") == 0) {
            client_port = port + 2;
        }
        
        client_server_addr.sin_port = htons(client_port);
        
        if (bind(client_server_socket, (struct sockaddr*)&client_server_addr, sizeof(client_server_addr)) < 0) {
            perror("bind failed");
            close(client_server_socket);
            return -1;
        }
        
        if (listen(client_server_socket, 1) < 0) {
            perror("listen failed");
            close(client_server_socket);
            return -1;
        }
        
        server_socket = client_server_socket;
        
        if (pthread_create(&receiver_thread_id, NULL, receiver_thread, NULL) != 0) {
            perror("thread creation failed");
            close(server_socket);
            return -1;
        }
        
        printf("%s: Client server listening on port %d\n", proc_name, client_port);
    }
    
    return 0;
}

void close_stub() {
    is_running = 0;
    pthread_cancel(receiver_thread_id);
    pthread_join(receiver_thread_id, NULL);
    close(server_socket);
    pthread_mutex_destroy(&msg_queue.mutex);
}

int send_message_to_process(const char* target_process, enum operations action) {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    /* Determinar el puerto destino basado en el proceso objetivo */
    int target_port = server_port; // Por defecto, puerto de P2
    
    if (strcmp(process_name, "P2") == 0) {
        /* P2 envía a P1 o P3 en sus puertos específicos */
        if (strcmp(target_process, "P1") == 0) {
            target_port = server_port + 1; // P1 en puerto+1
        } else if (strcmp(target_process, "P3") == 0) {
            target_port = server_port + 2; // P3 en puerto+2
        }
    } else {
        /* P1 y P3 siempre envían a P2 */
        target_port = server_port;
    }
    
    server_addr.sin_port = htons(target_port);
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Cannot connect to %s at port %d\n", target_process, target_port);
        close(client_socket);
        return -1;
    }
    
    increment_clock_for_send();
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
    } else {
        printf("Error: Failed to send message to %s\n", target_process);
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