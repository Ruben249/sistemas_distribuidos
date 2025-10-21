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
    
    printf("Starting P1 on %s:%s\n", argv[1], argv[2]);
    
    if (init_stub("P1", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }
    
    // Enviar READY inmediatamente (reloj 1)
    increment_clock();
    send_message(P2_IP, P2_PORT, READY_TO_SHUTDOWN);
    
    // Esperar SHUTDOWN de P2 - evento en reloj 5
    while (get_clock_lamport() < 5) {
        process_pending_messages();
    }
    
    // Enviar ACK a P2 en reloj 6
    while (get_clock_lamport() < 6) {
        process_pending_messages();
    }
    increment_clock();
    send_message(P2_IP, P2_PORT, SHUTDOWN_ACK);
    
    printf("P1: Shutdown completed at lamport time %d\n", get_clock_lamport());
    close_stub();
    return 0;
}