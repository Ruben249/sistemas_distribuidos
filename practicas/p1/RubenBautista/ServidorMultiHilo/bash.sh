#!/bin/bash
# Script para lanzar múltiples clientes simultáneamente
# Uso: ./run_clients.sh <num_clientes> [ip_servidor] [puerto]
# Ejemplo: ./run_clients.sh 300 127.0.0.1 8000

NUM_CLIENTS=${1:-300}
SERVER_IP=${2:-127.0.0.1}
SERVER_PORT=${3:-8000}

echo "Lanzando $NUM_CLIENTS clientes contra $SERVER_IP:$SERVER_PORT..."

for i in $(seq 1 "$NUM_CLIENTS"); do
WAIT=$(printf '0.%06d\n' $RANDOM)
(
sleep "$WAIT"
echo "Lanzando cliente $i ..."
./client "$i" "$SERVER_IP" "$SERVER_PORT"
) &
done

# Esperar a que terminen todos los clientes
wait
echo "Todos los clientes han finalizado."