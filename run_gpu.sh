#!/bin/sh

cd build || exit 1
make -j "$(nproc)" GpuTest || exit 1
cd bin || exit 1
rm result.png 2>/dev/null
./GpuTest || exit 1
xdg-open result.png
