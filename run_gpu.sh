#!/bin/sh

cd build || return 1
make -j "$(nproc)" GpuTest || return 1
cd bin || return 1
rm result.png 2>/dev/null
./GpuTest
xdg-open result.png
