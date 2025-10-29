#include "stub.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <my_ip> <my_port>\n", argv[0]);
        return 1;
    }
    
    if (init_stub("P2", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }

    // Wait for READY messages from P1 and P3
    wait_for_ready_messages();

    // Send SHUTDOWN to P1 (usa las variables globales directamente)
    send_message(p1_ip, p1_port, SHUTDOWN_NOW);
    
    // Wait for ACK from P1
    while (get_clock_lamport() != 7) {
        usleep(SLEEP_INTERVAL);
    }
    
    // Send SHUTDOWN to P3 (usa las variables globales directamente)
    send_message(p3_ip, p3_port, SHUTDOWN_NOW);
    
    // Wait for ACK from P3
    while (get_clock_lamport() != 11) {
        usleep(SLEEP_INTERVAL);
    }
    
    printf("Los clientes fueron correctamente apagados en t(lamport) = %d\n", get_clock_lamport());
    close_stub();
    return 0;
}