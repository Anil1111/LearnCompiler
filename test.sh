#!/bin/bash

try_parser() {
    ./cp_tkn "$2" > tmp.txt
    diff --ignore-space-change tmp.txt "$1"
    if [ $? -ne 0 ]; then
        echo "cp_tkn: Output is different from expectation."
        exit 1
    fi
}

try_polish() {
    echo -e "$2" | ./polish > tmp.txt
    echo -e "$1" > target.txt
    diff --ignore-all-space --strip-trailing-cr tmp.txt target.txt
    if [ $? -ne 0 ]; then
        echo "polish: Output is different from expectation."
        exit 1
    fi
}

try_minicalc() {
    echo -e "$2" | ./minicalc > tmp.txt
    echo -e "$1" > expectation.txt
    diff --ignore-all-space --strip-trailing-cr tmp.txt expectation.txt
    if [ $? -ne 0 ]; then
        echo "minicalc: Output is different from expectation."
        exit 1
    fi
}

try_parser "token_parsed.txt" "token_txt.txt"

try_polish "Input: Converted: ab+c+ \n Result: 6" a+b+c
try_polish "Input: Converted: abc*+ \n Result: 7" a+b*c
try_polish "Input: Converted: ab+5* \n Result: 15" "(a+b)*5"

try_minicalc "610 \n 900 \n 5" "a=10\nb=20\nc=a+b*30\n?c\nc=(a+b)*30\n?c\n? 1+2+3+4-5\n"

echo OK
