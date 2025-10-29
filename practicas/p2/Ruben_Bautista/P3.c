#include "stub.h"

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printf("Usage: %s <my_ip> <my_port> <p2_ip> <p2_port>\n", argv[0]);
        return 1;
    }
    
    // Guardar info de P2 desde argumentos
    char p2_ip[16];
    int p2_port;
    strcpy(p2_ip, argv[3]);
    p2_port = atoi(argv[4]);
    
    if (init_stub("P3", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }
    
    // P3 sends READY to P2
    send_message(p2_ip, p2_port, READY_TO_SHUTDOWN);
    
    // P3 waits to receive SHUTDOWN
    while (get_clock_lamport() != 9) {
        usleep(SLEEP_INTERVAL);
    }
    
    // P3 sends ACK to P2
    send_message(p2_ip, p2_port, SHUTDOWN_ACK);
    
    close_stub();
    return 0;
}