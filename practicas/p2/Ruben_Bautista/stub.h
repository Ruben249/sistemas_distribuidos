#ifndef STUB_H
#define STUB_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_PROCESS_NAME 20
#define MAX_MESSAGE_QUEUE 100
#define SLEEP_INTERVAL 100000
#define CONNECTION_RETRIES 10
#define RETRY_DELAY 500000

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

// P2 specific functions and variables
int wait_for_ready_messages(void);
extern char p1_ip[16];
extern char p3_ip[16];
extern int p1_port;
extern int p3_port;

// Utility functions
const char* operation_to_string(enum operations op);

#endif