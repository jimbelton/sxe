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

#include <sxe-log.h>

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

#define SXE_YIELD() SwitchToThread()

#else

#include <sched.h>

/* The following inline function is based on ReactOS; the license in the source file is MIT
 * See: http://www.google.com/codesearch/p?hl=en#S3vzerue4i0/trunk/reactos/include/crt/mingw32/intrin_x86.h
 */

#define __INTRIN_INLINE extern __inline__ __attribute__((__always_inline__,__gnu_inline__))

#if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) > 40100

long InterlockedCompareExchange(volatile long * const Destination, const long Exchange, const long Comperand);

__INTRIN_INLINE long InterlockedCompareExchange(volatile long * const Destination, const long Exchange, const long Comperand)
{
    return __sync_val_compare_and_swap(Destination, Comperand, Exchange);
}

long InterlockedAdd(long volatile * Addend, long Value);

__INTRIN_INLINE long InterlockedAdd(long volatile * Addend, long Value) {
    return __sync_add_and_fetch(Addend, Value);
}

#define SXE_YIELD() sched_yield()

#else

#error "Time to upgrade your compiler - gcc --version must be > 4.1.0"

#endif
#endif

extern unsigned sxe_spinlock_count_max; /* number of yields before spinlock fails */

typedef struct SXE_SPINLOCK {
    volatile long lock;
} SXE_SPINLOCK;

#define SXE_SPINLOCK_INIT(spinlock, new) \
            spinlock.lock = new

#define SXE_SPINLOCK_RESET(spinlock, new) \
            InterlockedCompareExchange(&spinlock.lock, new, InterlockedCompareExchange(&spinlock.lock, new, new))

#define SXE_SPINLOCK_TAKE(spinlock, new, old, count) \
            count = 0; \
            /* SXEL73("SXE_SPINLOCK_TAKE(spinlock.lock=%u, new=%u, old=%u)", spinlock.lock, new, old); */ \
            while ((count < sxe_spinlock_count_max                                      )   \
            &&     (InterlockedCompareExchange(&spinlock.lock, new, old) != (long)(old))) { \
                count++; SXE_YIELD(); \
            } \
            /* SXEL71("SXE_SPINLOCK_TAKE // done; count=%u", count); */ \
            if(count >= sxe_spinlock_count_max) SXEL51("SXE_SPINLOCK_TAKE FAILED; reached sxe_spinlock_count_max (%u)", sxe_spinlock_count_max)

#define SXE_SPINLOCK_IS_TAKEN(count) \
            (count < sxe_spinlock_count_max)

#define SXE_SPINLOCK_GIVE(spinlock, new, old) \
            SXEA12(InterlockedCompareExchange(&spinlock.lock, new, old) == (long)(old), \
                "SXE_SPINLOCK_GIVE: Lock 0x%p got clobbered (expected %ld)", &spinlock.lock, new)

#endif
