#!/bin/bash

# Safer bash execution: 
# -e  exit if any command fails
# -u  error on use of undefined variables
# -o pipefail  fail if any command in a pipeline fails
set -euo pipefail

BIN=./alsarb

echo "Building project..."
make debug

echo
echo "Running mode tests..."
echo

echo "---------------------------------"
echo "Test: default pipeline mode"
$BIN
echo "PASS"
echo

echo "---------------------------------"
echo "Test: help (--help)"
$BIN --help
echo "PASS"
echo

echo "---------------------------------"
echo "Test: help (-h)"
$BIN -h
echo "PASS"
echo

echo "---------------------------------"
echo "Test: list-devices"
$BIN list-devices
echo "PASS"
echo

echo "---------------------------------"
echo "Test: ring buffer stress test"
$BIN test-ring
echo "PASS"
echo

echo "---------------------------------"
echo "Test: unknown mode (should fail)"
if $BIN nonsense; then
    echo "FAIL: expected non-zero exit"
    exit 1
else
    echo "PASS"
fi
echo

echo "---------------------------------"
echo "All mode tests completed."