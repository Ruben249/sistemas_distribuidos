#ifndef STUB_H
#define STUB_H

#define MAX_PROCESS_NAME 20
#define MAX_MESSAGE_QUEUE 100
#define SLEEP_TIME 100000
#define MAX_CLIENTS 2

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

int init_stub(const char* process_name, const char* ip, int port);
void close_stub();
int get_clock_lamport();
int send_message_to_process(const char* process_name, enum operations action);
int wait_for_ready_messages(void);
int has_pending_message(void);
int receive_message(struct message* msg);
void reset_clock(void);

#endif