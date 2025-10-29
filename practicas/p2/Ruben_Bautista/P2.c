#include "stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define P1_IP "127.0.0.1"
#define P3_IP "127.0.0.1" 
#define P1_PORT 8001
#define P3_PORT 8003

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }
    
    if (init_stub("P2", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }
    
    int p1_ready_received = 0;
    int p3_ready_received = 0;
    struct message received_msg;
    
    printf("P2: Waiting for READY messages from P1 and P3...\n");
    
    // Wait for READY messages from P1 and P3
    while (!p1_ready_received || !p3_ready_received) {
        if (has_pending_message()) {
            // If P3's READY arrives before P1's, reset clock and show error
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
                        printf("P2: Received READY from P3 after P1 - correct order\n");
                    }
                }
            }
        }
        usleep(SLEEP_INTERVAL);
    }

    // Wait for both READY messages to be processed
    while (get_clock_lamport() != 3) {
        usleep(SLEEP_INTERVAL);
    }
    // Send SHUTDOWN to P1
    send_message(P1_IP, P1_PORT, SHUTDOWN_NOW);
    
    // Wait for P1 to ACK
    while (get_clock_lamport() != 7) {
        usleep(SLEEP_INTERVAL);
    }
    // Send SHUTDOWN to P3
    send_message(P3_IP, P3_PORT, SHUTDOWN_NOW);
    
    // Wait for P3 to ACK
    while (get_clock_lamport() != 11) {
        usleep(SLEEP_INTERVAL);
    }
    
    printf("Los clientes fueron correctamente apagados en t(lamport) = %d\n", 
           get_clock_lamport());
    close_stub();
    return 0;
}