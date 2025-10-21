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
    
    printf("Starting P2 on %s:%s\n", argv[1], argv[2]);
    
    if (init_stub("P2", argv[1], atoi(argv[2])) != 0) {
        fprintf(stderr, "Failed to initialize stub\n");
        return 1;
    }
    
    // SECUENCIA BASADA EXCLUSIVAMENTE EN RELOJES LAMPORT
    // P2 espera a que el reloj alcance ciertos valores
    
    // Esperar a recibir ambos READY (P1 y P3) - eventos en reloj 2 y 3
    while (get_clock_lamport() < 3) {
        process_pending_messages();
    }
    
    // Enviar SHUTDOWN a P1 en reloj 4
    while (get_clock_lamport() < 4) {
        process_pending_messages();
    }
    increment_clock();
    send_message(P1_IP, P1_PORT, SHUTDOWN_NOW);
    
    // Esperar ACK de P1 - evento en reloj 7
    while (get_clock_lamport() < 7) {
        process_pending_messages();
    }
    
    // Enviar SHUTDOWN a P3 en reloj 8
    while (get_clock_lamport() < 8) {
        process_pending_messages();
    }
    increment_clock();
    send_message(P3_IP, P3_PORT, SHUTDOWN_NOW);
    
    // Esperar ACK de P3 - evento en reloj 11
    while (get_clock_lamport() < 11) {
        process_pending_messages();
    }
    
    printf("Los clientes fueron correctamente apagados en t(lamport) = %d\n", 
           get_clock_lamport());
    close_stub();
    return 0;
}