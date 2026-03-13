#!/bin/bash

# Safer bash execution: 
# -e  exit if any command fails
# -u  error on use of undefined variables
# -o pipefail  fail if any command in a pipeline fails
set -euo pipefail

BIN=./alsarb
VALGRIND="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes"

if ! command -v valgrind >/dev/null 2>&1; then
    echo "Valgrind not installed."
    echo "Install with:"
    echo "  sudo apt update"
    echo "  sudo apt install valgrind"
    exit 1
fi

echo "Building debug binary..."
make clean
make debug

echo
echo "Running ring buffer test under Valgrind..."
echo

$VALGRIND $BIN test-ring