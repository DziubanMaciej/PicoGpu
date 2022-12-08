#!/bin/sh

test="$1"
if [ -z "$test" ]; then
    echo "ERROR: specify name of the test."
    exit 1
fi

cd build || exit 1
make -j $(nproc) "$test" || exit 1
cd bin || exit 1
./"$test"
echo "success_code=$?"