/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * jwdshim.so - compatibility shim for the 2004 JustSystems Wine fork ("jwd")
 * on current Linux kernels/glibc.  Build:  gcc -m32 -shared -fPIC -O2 -o jwdshim.so jwdshim.c -ldl
 *
 * 1. Old Wine reserves the first megabyte for DOS/Win16 support by calling
 *    mmap(0x1000, ...) *without* MAP_FIXED and expects the hint to be honoured.
 *    Current kernels round any hint below mmap_min_addr upwards, so the request
 *    lands elsewhere and winedos later writes to an unmapped fixed address.
 *    -> turn the low hint into MAP_FIXED_NOREPLACE; if the kernel refuses pages
 *       below vm.mmap_min_addr, map 0x10000-0x110000 only and pretend success
 *       (Wine keeps the first 64K inaccessible for Windows programs anyway).
 * 2. GlobalMemoryStatus() computes "MemTotal_kB * 1024" in a 32-bit int.
 *    With >2 GB RAM the installer aborts with "not enough memory".
 *    -> serve a small fake /proc/meminfo.
 * 3. Each thread gets one mmap of 0x1f84 bytes: the TEB page followed by a
 *    4096-byte signal stack.  Handlers find the TEB as "(esp & ~4095) - 4096".
 *    Signal frames now carry the XSAVE area (AVX, AVX-512 ...) and take most
 *    or all of that page, so handlers run off its bottom into the TEB and
 *    corrupt its last fields (e.g. the OLE apartment pointer).
 *    -> enlarge the mapping and extend the signal stack *upwards* by the
 *       measured size of a signal frame, so the kernel's frame sits above the
 *       original page and the handler gets the whole page to itself again,
 *       still directly above the TEB.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <signal.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static const char fake_meminfo[] =
    "MemTotal:        1048576 kB\n"
    "MemFree:          786432 kB\n"
    "Buffers:           16384 kB\n"
    "Cached:           131072 kB\n"
    "SwapTotal:        524288 kB\n"
    "SwapFree:         524288 kB\n";

#define LOW_LIMIT 0x10000UL   /* default vm.mmap_min_addr */
#define DOS_END   0x110000UL  /* end of the area old Wine reserves for DOS memory */

#define TEB_MAP_LEN     0x1f84UL   /* SIGNAL_STACK_SIZE + sizeof(TEB) in this Wine */
#define TEB_MAP_PAGES   0x2000UL
#define SIGSTACK_EXTRA  0x10000UL  /* room for the kernel's signal frame */

static void teb_register(void *teb);

static void *raw_mmap2(void *addr, size_t len, int prot, int flags, int fd, unsigned long pgoff)
{
    return (void *)syscall(SYS_mmap2, addr, len, prot, flags, fd, pgoff);
}

static void *do_mmap2(void *addr, size_t len, int prot, int flags, int fd, unsigned long pgoff)
{
    uintptr_t a = (uintptr_t)addr;

    /* alloc_teb(): TEB + signal stack, see below */
    if (!a && len == TEB_MAP_LEN && (flags & (MAP_ANONYMOUS | MAP_FIXED)) == MAP_ANONYMOUS)
    {
        void *p = raw_mmap2(NULL, TEB_MAP_PAGES + SIGSTACK_EXTRA, prot, flags, fd, pgoff);
        if (p != MAP_FAILED) { teb_register(p); return p; }
    }
    /* step 1 of setup_dos_mem(): mmap(0x1000, 0x10f000) as a plain hint */
    if (a && a < LOW_LIMIT && !(flags & MAP_FIXED) && (flags & MAP_ANONYMOUS) && a + len <= DOS_END)
    {
        void *p = raw_mmap2(addr, len, prot, flags | MAP_FIXED_NOREPLACE, fd, pgoff);
        if (p != MAP_FAILED) return p;
        /* not allowed below vm.mmap_min_addr: take what we may have and
         * report success; only the first 64K (kept inaccessible by Wine
         * for Windows programs anyway) stays unmapped */
        p = raw_mmap2((void *)LOW_LIMIT, a + len - LOW_LIMIT, prot, flags | MAP_FIXED_NOREPLACE, fd, pgoff);
        if (p == (void *)LOW_LIMIT) return addr;
    }
    /* step 2: mmap(NULL, 0x110000, MAP_FIXED) to cover page zero as well */
    else if (!a && (flags & MAP_FIXED) && (flags & MAP_ANONYMOUS) && len > LOW_LIMIT && len <= DOS_END)
    {
        void *p = raw_mmap2(addr, len, prot, flags, fd, pgoff);
        if (p != MAP_FAILED) return p;
        p = raw_mmap2((void *)LOW_LIMIT, len - LOW_LIMIT, prot, flags, fd, pgoff);
        if (p == (void *)LOW_LIMIT) return NULL;
    }
    return raw_mmap2(addr, len, prot, flags, fd, pgoff);
}

/* protecting the (possibly unmapped) first 64K must not fail */
int mprotect(void *addr, size_t len, int prot)
{
    int ret = syscall(SYS_mprotect, addr, len, prot);
    if (ret && (uintptr_t)addr + len <= LOW_LIMIT) return 0;
    return ret;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, long off)
{
    return do_mmap2(addr, len, prot, flags, fd, (unsigned long)off >> 12);
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd, long long off)
{
    return do_mmap2(addr, len, prot, flags, fd, (unsigned long)(off >> 12));
}

static FILE *fake_file(void)
{
    return fmemopen((void *)fake_meminfo, sizeof(fake_meminfo) - 1, "r");
}

FILE *fopen(const char *path, const char *mode)
{
    static FILE *(*real)(const char *, const char *);
    if (path && !strcmp(path, "/proc/meminfo")) return fake_file();
    if (!real) real = dlsym(RTLD_NEXT, "fopen");
    return real(path, mode);
}

FILE *fopen64(const char *path, const char *mode)
{
    static FILE *(*real)(const char *, const char *);
    if (path && !strcmp(path, "/proc/meminfo")) return fake_file();
    if (!real) real = dlsym(RTLD_NEXT, "fopen64");
    return real(path, mode);
}


/* ---- signal stack / TEB layout ----------------------------------------- */

#define TEB_SLOTS 1024

static void *tebs[TEB_SLOTS];
static volatile int teb_lock;
static unsigned long frame_extra;   /* bytes added on top of Wine's signal stack */

static void teb_register(void *teb)
{
    int i;
    while (__sync_lock_test_and_set(&teb_lock, 1)) ;
    for (i = 0; i < TEB_SLOTS; i++) if (!tebs[i] || tebs[i] == teb) { tebs[i] = teb; break; }
    __sync_lock_release(&teb_lock);
}

static int teb_lookup(void *teb, int remove)
{
    int i, found = 0;
    while (__sync_lock_test_and_set(&teb_lock, 1)) ;
    for (i = 0; i < TEB_SLOTS; i++)
        if (tebs[i] == teb) { found = 1; if (remove) tebs[i] = NULL; break; }
    __sync_lock_release(&teb_lock);
    return found;
}

int munmap(void *addr, size_t len)
{
    if (addr && len <= TEB_MAP_PAGES && teb_lookup(addr, 1)) len = TEB_MAP_PAGES + SIGSTACK_EXTRA;
    return syscall(SYS_munmap, addr, len);
}

int sigaltstack(const stack_t *ss, stack_t *old)
{
    stack_t big;

    if (ss && !(ss->ss_flags & SS_DISABLE) && ss->ss_size == 4096 && frame_extra &&
        teb_lookup((char *)ss->ss_sp - 4096, 0))
    {
        big = *ss;
        big.ss_size = 4096 + frame_extra;
        ss = &big;
    }
    return syscall(SYS_sigaltstack, ss, old);
}

/* Measure how far below the top of an alternate stack a (non-SA_SIGINFO)
 * handler starts running on this CPU/kernel. */
static volatile uintptr_t probe_esp;

static void probe_handler(int sig)
{
    probe_esp = (uintptr_t)&sig - 4;   /* %esp on entry: return address, then sig */
}

static unsigned long measure_frame(unsigned long top_off)
{
    const size_t area = 0x20000;
    struct sigaction sa, old_sa;
    stack_t st, old_st;
    sigset_t set, old_set;
    unsigned long ret = 0;
    char *base = raw_mmap2(NULL, area + SIGSTACK_EXTRA, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (base == MAP_FAILED) return 0;
    st.ss_sp = base;
    st.ss_size = area + top_off;
    st.ss_flags = 0;
    if (syscall(SYS_sigaltstack, &st, &old_st)) goto done;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = probe_handler;
    sa.sa_flags = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR2, &sa, &old_sa);
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigprocmask(SIG_UNBLOCK, &set, &old_set);

    probe_esp = 0;
    raise(SIGUSR2);
    if (probe_esp) ret = (uintptr_t)base + area + top_off - probe_esp;

    sigprocmask(SIG_SETMASK, &old_set, NULL);
    sigaction(SIGUSR2, &old_sa, NULL);
    if (old_st.ss_flags & SS_DISABLE) { old_st.ss_sp = NULL; old_st.ss_size = 0; }
    syscall(SYS_sigaltstack, &old_st, NULL);
done:
    syscall(SYS_munmap, base, area + SIGSTACK_EXTRA);
    return ret;
}

static void __attribute__((constructor)) jwdshim_init(void)
{
    unsigned long frame, extra;
    int i;

    /* Put the handler's entry %esp 64..127 bytes below the top of Wine's
     * original stack page.  The frame size depends on the alignment of the
     * stack top, so re-measure at the chosen offset until it is stable. */
    frame = measure_frame(0);
    extra = 0;
    for (i = 0; frame && i < 4; i++)
    {
        unsigned long want = frame > 128 ? (frame - 64) & ~63UL : 0;
        if (want > SIGSTACK_EXTRA) want = 0;      /* does not fit: leave alone */
        if (want == extra) break;
        extra = want;
        if (!extra) break;
        frame = measure_frame(extra);
    }
    frame_extra = extra;
    if (getenv("JWDSHIM_DEBUG"))
        fprintf(stderr, "jwdshim: signal frame %lu bytes, signal stack extended by %lu\n", frame, extra);
}
