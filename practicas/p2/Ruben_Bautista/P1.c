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
    
    // P1 envía READY inmediatamente (reloj se incrementa a 1)
    send_message(P2_IP, P2_PORT, READY_TO_SHUTDOWN);
    
    // P1 espera recibir SHUTDOWN (reloj debe ser 5 después de recibir)
    while (get_clock_lamport() < 5) {
        usleep(SLEEP_INTERVAL);
    }
    
    // P1 envía ACK (reloj se incrementa a 6)
    send_message(P2_IP, P2_PORT, SHUTDOWN_ACK);
    close_stub();
    return 0;
}