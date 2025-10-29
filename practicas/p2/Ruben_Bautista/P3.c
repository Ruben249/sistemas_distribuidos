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
    
    if (init_stub("P3", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }
    
    // P3 sends READY to P2
    send_message(get_process_ip("P2"), get_process_port("P2"), READY_TO_SHUTDOWN);
    
    // P3 waits to receive SHUTDOWN
    while (get_clock_lamport() != 9) {
        usleep(SLEEP_INTERVAL);
    }
    
    // P3 sends ACK to P2
    send_message(get_process_ip("P2"), get_process_port("P2"), SHUTDOWN_ACK);
    
    close_stub();
    return 0;
}