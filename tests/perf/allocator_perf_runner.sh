#!/bin/bash

set -euo pipefail

echo "TEST_NAME=${TEST_NAME:=allocator.allocs_100}"

function run_bench {
    local size=$1
    printf "%-10s" "$size"
    ALLOC_SIZE=$size tests/perf/allocator_perf --blocked-reactor-notify-ms=999999 --smp=1 --memory=1G \
        --iterations=10000 2>seastar.log | grep "$TEST_NAME"
}

# print the header row
printf "%-10s" "SIZE"
tests/perf/allocator_perf --smp=1 --memory=1G --iterations=1 2>/dev/null | grep 'median'


for exp in {0..20}; do
    size=$((2**exp))
    run_bench $size
    run_bench $(("$size" * 3 / 2))
done

echo "DONE!"

