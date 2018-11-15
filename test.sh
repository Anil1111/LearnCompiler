#!/bin/bash

try_parser() {
    ./cp_tkn "$2" > tmp.txt
    diff tmp.txt "$1"
    if [ $? -ne 0 ]; then
        echo "Output is different from expectation."
        exit 1
    fi
}

try_parser "token_parsed.txt" "token_txt.txt"

echo OK
