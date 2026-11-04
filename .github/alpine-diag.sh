#!/bin/sh
# Print runner hardware/kernel facts and run the sigaltstack/mbind probe.
set -x
uname -a
cat /proc/cmdline
grep -m1 'model name' /proc/cpuinfo
grep -m1 -E '^(cpu family|model|stepping|microcode)' /proc/cpuinfo
grep -E '^(cpu family|model|stepping|microcode)\s' /proc/cpuinfo | sort -u
grep -m1 '^flags' /proc/cpuinfo | tr ' ' '\n' | grep -E 'avx512|amx|apx|avx10|xsave|pku' | tr '\n' ' '; echo
nproc
head -3 /proc/meminfo
cat /sys/fs/cgroup/memory.max /sys/fs/cgroup/cpuset.cpus.effective /sys/fs/cgroup/cpuset.mems.effective 2>&1
ls /sys/devices/system/node/ 2>&1
grep -E 'Seccomp|NoNewPrivs|Cap(Eff|Bnd)' /proc/self/status
apk info -v musl 2>/dev/null
set +x
gcc -O1 -o /tmp/alpine-probe "$(dirname "$0")/alpine-probe.c" && /tmp/alpine-probe
