#!/bin/bash

# Safer bash execution: 
# -e  exit if any command fails
# -u  error on use of undefined variables
# -o pipefail  fail if any command in a pipeline fails
set -euo pipefail

rm -f samples.wav amplified.wav
BIN=./alsarb

echo "Building project..."
make debug

echo
echo "Running mode tests..."
echo

echo "---------------------------------"
echo "Test: default pipeline mode"
$BIN

if [[ ! -f samples.wav ]]; then
    echo "FAIL: samples.wav not created"
    exit 1
fi

if [[ ! -f amplified.wav ]]; then
    echo "FAIL: amplified.wav not created"
    exit 1
fi

echo "PASS: capture.wav and amplified.wav created"
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