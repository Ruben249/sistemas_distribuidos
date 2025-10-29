#include "stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define P2_IP "127.0.0.1"
#define P2_PORT 8002

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }
    
    if (init_stub("P1", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }
    
    // P1 sends READY to P2
    send_message(P2_IP, P2_PORT, READY_TO_SHUTDOWN);
    
    // P1 waits to receive SHUTDOWN
    while (get_clock_lamport() != 5) {
        usleep(SLEEP_INTERVAL);
    }
    
    // P1 sends ACK
    send_message(P2_IP, P2_PORT, SHUTDOWN_ACK);
    close_stub();
    return 0;
}