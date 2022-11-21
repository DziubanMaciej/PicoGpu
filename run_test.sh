#!/bin/sh

test="$1"
debug="$2"
if [ -z "$test" ]; then
    echo "ERROR: specify name of the test."
    exit 1
fi

cd build || exit 1
make -j $(nproc) "$test" || exit 1
cd bin || exit 1
if [ "$debug" == "1" ]; then
    gdb ./"$test"
else
    ./"$test"
fi
echo "success_code=$?"
