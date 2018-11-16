#!/bin/bash

try_parser() {
    ./cp_tkn "$2" > tmp.txt
    diff --ignore-space-change tmp.txt "$1"
    if [ $? -ne 0 ]; then
        echo "Output is different from expectation."
        exit 1
    fi
}

try_polish() {
    yes "$2" | ./polish > tmp.txt
    echo -e "$1" > target.txt
    diff --ignore-all-space --strip-trailing-cr tmp.txt target.txt
    if [ $? -ne 0 ]; then
        echo "Output is different from expectation."
        exit 1
    fi
}

try_parser "token_parsed.txt" "token_txt.txt"

try_polish "Input: Converted: ab+c+ \n Result: 6" a+b+c
try_polish "Input: Converted: abc*+ \n Result: 7" a+b*c
try_polish "Input: Converted: ab+5* \n Result: 15" "(a+b)*5"

echo OK
