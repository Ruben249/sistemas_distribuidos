#include "stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }
    
    if (init_stub("P1", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }

    /* P1 sends READY to P2 */
    send_message_to_process("P2", READY_TO_SHUTDOWN);
    
    /* P1 waits to receive SHUTDOWN from P2 */
    while (get_clock_lamport() != 5) {
        usleep(SLEEP_TIME);
    }
    
    /* P1 sends ACK to P2 */
    send_message_to_process("P2", SHUTDOWN_ACK);
    
    close_stub();
    return 0;
}