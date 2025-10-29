#ifndef STUB_H
#define STUB_H

#include <pthread.h>
#include <signal.h>

#define MAX_PROCESS_NAME 20
#define MAX_IP_LENGTH 16
#define MAX_MESSAGE_QUEUE 100
#define SLEEP_INTERVAL 100000
#define CONNECTION_RETRIES 5
#define RETRY_DELAY 200000

enum operations {
    READY_TO_SHUTDOWN = 0,
    SHUTDOWN_NOW,
    SHUTDOWN_ACK
};

struct message {
    char origin[MAX_PROCESS_NAME];
    enum operations action;
    unsigned int clock_lamport;
};

struct client_data {
    int client_fd;
    char process_name[MAX_PROCESS_NAME];
};

// Stub initialization and cleanup
int init_stub(const char* process_name, const char* ip, int port);
void close_stub();

// Lamport clock functions
int get_clock_lamport();
void reset_clock();

// Message handling functions
int send_message(const char* dest_ip, int dest_port, enum operations action);
int has_pending_message();
int receive_message(struct message* msg);

// Utility functions
const char* operation_to_string(enum operations op);

#endif