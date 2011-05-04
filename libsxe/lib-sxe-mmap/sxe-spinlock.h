/* Copyright (c) 2010 Sophos Group.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __SXE_SPINLOCK__
#define __SXE_SPINLOCK__

#include <unistd.h>
#include "sxe-log.h"

/* If using Windows
 */
#ifdef _WIN32

#include <winsock2.h>
#include <windows.h>

/*
 * LONG __cdecl InterlockedCompareExchange(
 *      __inout LONG volatile *Destination,
 *      __in    LONG Exchange,
 *      __in    LONG Comparand);
 */

#define SXE_GETTID() GetCurrentThreadId()
#define SXE_YIELD()  SwitchToThread()

#else

#include <sched.h>
#include <sys/types.h>
#include <sys/syscall.h>

#ifdef __FreeBSD__
#include <sys/thr.h>
#endif

/* The following inline function is based on ReactOS; the license in the source file is MIT
 * See: http://www.google.com/codesearch/p?hl=en#S3vzerue4i0/trunk/reactos/include/crt/mingw32/intrin_x86.h
 */

#define __INTRIN_INLINE extern __inline__ __attribute__((__always_inline__,__gnu_inline__))

#if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) > 40100

static inline long
InterlockedCompareExchange(volatile long * const Destination, const long Exchange, const long Comperand)
{
    return __sync_val_compare_and_swap(Destination, Comperand, Exchange);
}

static inline long
InterlockedExchangeAdd(long volatile * Addend, long Value)
{
    return __sync_add_and_fetch(Addend, Value);
}

#ifdef __APPLE__
#define SXE_GETTID() syscall(SYS_thread_selfid)
#elif defined(__FreeBSD__)
static inline long
sxe_gettid(void)
{
    long tid;
    thr_self(&tid);
    return tid;
}
#define SXE_GETTID() sxe_gettid()
#else
#define SXE_GETTID() syscall(SYS_gettid)
#endif

#define SXE_YIELD()  sched_yield()

#else

#error "Time to upgrade your compiler - gcc --version must be > 4.1.0"

#endif
#endif

extern unsigned sxe_spinlock_count_max; /* number of yields before spinlock fails */

typedef enum SXE_SPINLOCK_STATUS {
    SXE_SPINLOCK_STATUS_NOT_TAKEN,       /* Unable to take lock (maximum spin count reached)        */
    SXE_SPINLOCK_STATUS_TAKEN,           /* Took the lock                                           */
    SXE_SPINLOCK_STATUS_ALREADY_TAKEN    /* This thread already has the lock - don't double unlock! */
}  SXE_SPINLOCK_STATUS;

typedef struct SXE_SPINLOCK {
    volatile long lock;
} SXE_SPINLOCK;

static inline void
sxe_spinlock_construct(SXE_SPINLOCK * spinlock)
{
    spinlock->lock = 0;
}

static inline void
sxe_spinlock_force(SXE_SPINLOCK * spinlock, long tid)
{
    InterlockedCompareExchange(&spinlock->lock, tid, InterlockedCompareExchange(&spinlock->lock, tid, tid));
}

static inline SXE_SPINLOCK_STATUS
sxe_spinlock_take(SXE_SPINLOCK * spinlock)
{
    unsigned count   = 0;
    long     our_tid = SXE_GETTID();
    long     old_tid;

    /* SXEL73("SXE_SPINLOCK_TAKE(spinlock.lock=%u, new=%u, old=%u)", spinlock.lock, new, old); */

    while ((count < sxe_spinlock_count_max) && ((old_tid = InterlockedCompareExchange(&spinlock->lock, our_tid, 0)) != 0)) {
        if (old_tid == our_tid) {
            SXEL52("sxe_spinlock_take: Spinlock %p already held by our tid %ld", &spinlock->lock, our_tid);
            return SXE_SPINLOCK_STATUS_ALREADY_TAKEN;
        }

        count++;
        SXE_YIELD();
    }

    /* SXEL71("SXE_SPINLOCK_TAKE // done; count=%u", count); */

    if (count >= sxe_spinlock_count_max) {
        SXEL21("sxe_spinlock_take failed: reached sxe_spinlock_count_max (%u)", sxe_spinlock_count_max);
        return SXE_SPINLOCK_STATUS_NOT_TAKEN;
    }

    return SXE_SPINLOCK_STATUS_TAKEN;
}

static inline void
sxe_spinlock_give(SXE_SPINLOCK * spinlock)
{
    long our_tid = SXE_GETTID();
    long old_tid;

    SXEA13((old_tid = InterlockedCompareExchange(&spinlock->lock, 0, our_tid)) == our_tid,
           "sxe_spinlock_give: Lock %p is held by thread %ld, not our tid %ld", &spinlock->lock, old_tid, our_tid);
}

#endif
