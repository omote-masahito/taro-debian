/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * shimtest - checks jwdrun and jwdshim.so without the product.
 *
 * It repeats, in a small 32-bit program, the things the 2004 Wine fork does
 * that no longer work on a current kernel/glibc, and checks that they work
 * again under the launcher and the shim.
 *
 * Run:  JWDDIR=<dir that contains lib/jwdshim.so> jwdrun ./shimtest
 *       (or "make check")
 */
#define _GNU_SOURCE
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/personality.h>

static int failures;

#define CHECK(cond, ...) \
    do { \
        if (cond) printf("ok    "); else { printf("FAIL  "); failures++; } \
        printf(__VA_ARGS__); putchar('\n'); \
    } while (0)

static sigjmp_buf segv_jmp;
static void on_segv(int sig) { siglongjmp(segv_jmp, sig); }

/* 1. personality set by jwdrun; code in a page mapped without PROT_EXEC runs */
static void test_personality(void)
{
    int per = personality(0xffffffff);
    unsigned char *code;
    volatile int ret = 0;

    CHECK((per & ADDR_COMPAT_LAYOUT) && (per & READ_IMPLIES_EXEC),
          "personality 0x%x has ADDR_COMPAT_LAYOUT and READ_IMPLIES_EXEC", per);

    code = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(code != MAP_FAILED && (uintptr_t)code < 0x80000000UL,
          "anonymous mmap returns an address below 2 GB (%p)", (void *)code);
    if (code == MAP_FAILED) return;

    memcpy(code, "\xb8\x2a\x00\x00\x00\xc3", 6);          /* mov $42,%eax; ret */
    signal(SIGSEGV, on_segv);
    if (!sigsetjmp(segv_jmp, 1)) ret = ((int (*)(void))code)();
    signal(SIGSEGV, SIG_DFL);
    CHECK(ret == 42, "code in a PROT_READ|PROT_WRITE page is executable");
    munmap(code, 4096);
}

/* 2. /proc/meminfo is replaced by a small fake one */
static void test_meminfo(void)
{
    char line[128];
    long total = -1;
    FILE *f = fopen("/proc/meminfo", "r");

    if (f)
    {
        while (fgets(line, sizeof(line), f))
            if (sscanf(line, "MemTotal: %ld kB", &total) == 1) break;
        fclose(f);
    }
    CHECK(total > 0 && total < 2 * 1024 * 1024,
          "/proc/meminfo reports MemTotal below 2 GB (%ld kB)", total);
}

/* 3. the first megabyte, reserved the way setup_dos_mem() does it */
static void test_dos_mem(void)
{
    const int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    volatile unsigned char *dos = (volatile unsigned char *)0xd0000;
    void *p;
    volatile int ok = 0;

    p = mmap((void *)0x1000, 0x10f000, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(p == (void *)0x1000, "mmap(0x1000, 0x10f000) without MAP_FIXED returns 0x1000 (%p)", p);

    p = mmap(NULL, 0x110000, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    CHECK(p == NULL, "mmap(NULL, 0x110000, MAP_FIXED) returns NULL (%p)", p);

    CHECK(mprotect(NULL, 0x10000, PROT_NONE) == 0, "mprotect() of the first 64K succeeds");

    signal(SIGSEGV, on_segv);
    if (!sigsetjmp(segv_jmp, 1)) { *dos = 0x5a; ok = (*dos == 0x5a); }
    signal(SIGSEGV, SIG_DFL);
    CHECK(ok, "address 0xd0000 is readable and writable");
}

/* 4. TEB page with a 4096-byte signal stack right above it */
#define TEB_MAP_LEN 0x1f84

static volatile uintptr_t handler_sp;

static void on_usr1(int sig)
{
    volatile char buf[3072];        /* a handler that needs most of the page */
    size_t i;

    for (i = 0; i < sizeof(buf); i++) buf[i] = 0x5a;
    handler_sp = (uintptr_t)buf;
}

static void test_teb(void)
{
    struct sigaction sa;
    stack_t ss, cur;
    unsigned char *teb;
    size_t i;
    int clean = 1;

    teb = mmap(NULL, TEB_MAP_LEN, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(teb != MAP_FAILED, "TEB-sized mmap succeeds");
    if (teb == MAP_FAILED) return;
    memset(teb, 0xa5, 4096);

    ss.ss_sp = teb + 4096;
    ss.ss_size = 4096;
    ss.ss_flags = 0;
    CHECK(sigaltstack(&ss, NULL) == 0, "sigaltstack() on the page above the TEB");
    sigaltstack(NULL, &cur);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_usr1;
    sa.sa_flags = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    raise(SIGUSR1);

    CHECK(handler_sp && ((handler_sp & ~4095UL) - 4096) == (uintptr_t)teb,
          "handler runs in the page directly above the TEB (signal stack is %lu bytes)",
          (unsigned long)cur.ss_size);
    for (i = 0; i < 4096; i++) if (teb[i] != 0xa5) { clean = 0; break; }
    CHECK(clean, "TEB page is intact after a signal");

    ss.ss_flags = SS_DISABLE;
    sigaltstack(&ss, NULL);
    signal(SIGUSR1, SIG_DFL);
    CHECK(munmap(teb, TEB_MAP_LEN) == 0, "munmap() of the TEB");
    CHECK(msync(teb + 0x2000, 4096, MS_ASYNC) == -1 && errno == ENOMEM,
          "nothing is left mapped above the TEB");
}

int main(void)
{
    const char *preload = getenv("LD_PRELOAD");

    setvbuf(stdout, NULL, _IONBF, 0);
    if (sizeof(void *) != 4) { fprintf(stderr, "shimtest must be built with -m32\n"); return 2; }
    if (!preload || !strstr(preload, "jwdshim.so"))
    {
        fprintf(stderr, "run as: JWDDIR=<dir with lib/jwdshim.so> jwdrun ./shimtest\n");
        return 2;
    }
    test_personality();
    test_meminfo();
    test_dos_mem();
    test_teb();
    printf("%s\n", failures ? "FAILED" : "all tests passed");
    return failures ? 1 : 0;
}
