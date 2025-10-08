#!/bin/bash

# Simple multiclient test script
# Usage: ./test_simple.sh <num_clients>

if [ $# -ne 1 ]; then
    echo "Usage: $0 <num_clients>"
    echo "Example: $0 300"
    exit 1
fi

NUM_CLIENTS=$1

echo "Testing with $NUM_CLIENTS clients"

for i in $(seq 1 $NUM_CLIENTS); do
    ./client $i 127.0.0.1 8080 &
done

wait
echo "All $NUM_CLIENTS clients completed"