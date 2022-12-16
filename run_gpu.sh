#!/bin/sh

debug="$1"

rm build/bin/result.png 2>/dev/null
./run_test.sh GpuTest "$debug" || return 1
xdg-open build/bin/result.png >/dev/null 2>&1
