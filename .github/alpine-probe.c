// Probe what the kernel allows for sigaltstack and mbind on this runner.
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef AT_MINSIGSTKSZ
#define AT_MINSIGSTKSZ 51
#endif

static void try_altstack(size_t size) {
    void* mem = malloc(size);
    stack_t st = {.ss_sp = mem, .ss_flags = 0, .ss_size = size};
    stack_t prev;
    int r = sigaltstack(&st, &prev);
    printf("PROBE sigaltstack size=%zu -> %s\n", size, r == 0 ? "ok" : strerror(errno));
    if (r == 0) {
        sigaltstack(&prev, NULL);
    }
    free(mem);
}

int main(void) {
    printf("PROBE SIGSTKSZ=%ld MINSIGSTKSZ=%ld\n", (long)SIGSTKSZ, (long)MINSIGSTKSZ);
    printf("PROBE AT_MINSIGSTKSZ=%lu\n", getauxval(AT_MINSIGSTKSZ));
#ifdef _SC_SIGSTKSZ
    printf("PROBE sysconf(_SC_SIGSTKSZ)=%ld sysconf(_SC_MINSIGSTKSZ)=%ld\n",
           sysconf(_SC_SIGSTKSZ), sysconf(_SC_MINSIGSTKSZ));
#endif
    size_t sizes[] = {2048, 4096, 8192, 10240, 12288, 16384, 32768, 65536};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        try_altstack(sizes[i]);
    }

    size_t len = 2 << 20;
    void* p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    unsigned long nodemask = 1;
    // MPOL_PREFERRED = 1, MPOL_BIND = 2
    long r = syscall(SYS_mbind, p, len, 2, &nodemask, sizeof(nodemask) * 8, 0);
    printf("PROBE mbind(MPOL_BIND, node0) -> %s\n", r == 0 ? "ok" : strerror(errno));
    r = syscall(SYS_mbind, p, len, 1, &nodemask, sizeof(nodemask) * 8, 0);
    printf("PROBE mbind(MPOL_PREFERRED, node0) -> %s\n", r == 0 ? "ok" : strerror(errno));
    return 0;
}
