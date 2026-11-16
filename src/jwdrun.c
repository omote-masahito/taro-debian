/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * jwdrun - 32-bit launcher for the 2004 JustSystems Wine fork ("jwd").
 * Build:  gcc -m32 -O2 -o jwdrun jwdrun.c
 * Usage:  jwdrun /opt/jwd/bin/wine [args...]
 *
 * Sets the process personality the old loader assumes, then execs it:
 *   ADDR_COMPAT_LAYOUT  bottom-up mmap, so allocations stay below 2 GB
 *   READ_IMPLIES_EXEC   pre-NX behaviour (Wine thunks live in data pages)
 * This must be done from a 32-bit process: exec'ing any 64-bit program
 * (env, sh, setarch ...) in between clears READ_IMPLIES_EXEC again.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/personality.h>

int main(int argc, char **argv)
{
    const char *dir = getenv("JWDDIR");
    char shim[4096];

    if (argc < 2) { fprintf(stderr, "usage: %s program [args...]\n", argv[0]); return 2; }
    if (!dir) dir = "/opt/jwd";
    snprintf(shim, sizeof(shim), "%s/lib/jwdshim.so", dir);

    if (personality(PER_LINUX32 | ADDR_COMPAT_LAYOUT | READ_IMPLIES_EXEC) == -1)
        perror("personality");
    setenv("LD_PRELOAD", shim, 1);
    /* glibc >= 2.41: let dlopen() load objects marked as needing an executable stack */
    setenv("GLIBC_TUNABLES", "glibc.rtld.execstack=2", 1);
    execv(argv[1], argv + 1);
    perror(argv[1]);
    return 127;
}
