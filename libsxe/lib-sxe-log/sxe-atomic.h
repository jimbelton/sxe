/* Copyright (c) 2022 Jim Belton
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

#ifndef __SXE_ATOMIC__
#define __SXE_ATOMIC__

/* If using Windows
 */
#ifdef _WIN32

#include <winsock2.h>
#include <windows.h>

/* See https://docs.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedexchangeadd */
 
static inline uint32_t
sxe_atomic_add32(uint32_t volatile *to, uint32_t amount)
{
    return (uint32_t)InterlockedExchangeAdd((LONG volatile *)to, (LONG)amount);
}

static inline uint64_t
sxe_atomic_add64(uint64_t volatile *to, uint64_t amount)
{
    return (uint64_t)InterlockedExchangeAdd64((LONG64 volatile *)to, (LONG64)amount);
}

static inline uint32_t
sxe_atomic_sub32(uint32_t volatile *from, uint32_t amount)
{
    return (uint32_t)InterlockedExchangeAdd((LONG volatile *)from, -(LONG)amount);
}

static inline uint64_t
sxe_atomic_sub64(uint64_t volatile *from, uint64_t amount)
{
    return (uint64_t)InterlockedExchangeAdd64((LONG64 volatile *)from, -(LONG64)amount);
}

/* Otherwise, assume gcc
 */
#else

/* See https://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html#g_t_005f_005fsync-Builtins */

static inline uint32_t
sxe_atomic_add32(uint32_t volatile *to, uint32_t amount)
{
    return __sync_add_and_fetch(to, amount);
}

static inline uint64_t
sxe_atomic_add64(uint64_t volatile *to, uint64_t amount)
{
    return __sync_add_and_fetch(to, amount);
}

static inline uint32_t
sxe_atomic_sub32(uint32_t volatile *from, uint32_t amount)
{
    return __sync_sub_and_fetch(from, amount);
}

static inline uint64_t
sxe_atomic_sub64(uint64_t volatile *from, uint64_t amount)
{
    return __sync_sub_and_fetch(from, amount);
}

#endif
#endif
