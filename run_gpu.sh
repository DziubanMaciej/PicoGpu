#!/bin/sh

debug="$1"

cd build || exit 1
make -j "$(nproc)" GpuTest || exit 1
cd bin || exit 1
rm result.png 2>/dev/null
export SC_COPYRIGHT_MESSAGE=DISABLE
if [ "$debug" == "1" ]; then
    gdb ./GpuTest || exit 1
else
    ./GpuTest || exit 1
    read _
    xdg-open result.png
fi
