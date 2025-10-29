#include "stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }
    setbuf(stdout, NULL);
    
    if (init_stub("P2", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }
    
    int p1_ready_received = 0;
    int p3_ready_received = 0;
    struct message received_msg;
    
    printf("P2: Waiting for READY messages from P1 and P3...\n");
    
    while (!p1_ready_received || !p3_ready_received) {
        if (has_pending_message()) {
            if (receive_message(&received_msg)) {
                if (strcmp(received_msg.origin, "P1") == 0 && received_msg.action == READY_TO_SHUTDOWN) {
                    p1_ready_received = 1;
                }
                else if (strcmp(received_msg.origin, "P3") == 0 && received_msg.action == READY_TO_SHUTDOWN) {
                    if (!p1_ready_received) {
                        printf("ERROR: P3 arrived before P1! Please execute P1 first\n");
                        reset_clock();
                        p3_ready_received = 0;
                    } else {
                        p3_ready_received = 1;
                    }
                }
            }
        }
        usleep(SLEEP_INTERVAL);
    }

    while (get_clock_lamport() != 3) {
        usleep(SLEEP_INTERVAL);
    }
    
    // Send SHUTDOWN to P1
    send_message(get_process_ip("P1"), get_process_port("P1"), SHUTDOWN_NOW);
    
    while (get_clock_lamport() != 7) {
        usleep(SLEEP_INTERVAL);
    }
    
    // Send SHUTDOWN to P3
    send_message(get_process_ip("P3"), get_process_port("P3"), SHUTDOWN_NOW);
    
    while (get_clock_lamport() != 11) {
        usleep(SLEEP_INTERVAL);
    }
    
    printf("Los clientes fueron correctamente apagados en t(lamport) = %d\n", get_clock_lamport());
    close_stub();
    return 0;
}