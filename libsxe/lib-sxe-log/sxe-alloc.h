/* Copyright (c) 2021 Jim Belton
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

#ifndef __SXE_ALLOC_H__
#define __SXE_ALLOC_H__ 1

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef SXE_FILE
#   ifdef MAK_FILE
#       define SXE_FILE MAK_FILE     // Normally, the mak system defines this as <component>/<package>/<file>.c
#   else
#       define SXE_FILE __FILE__
#   endif
#endif

/* Options/counters supported by the default implementation
 */
extern bool     sxe_alloc_diagnostics;         // Set to true to output informational diagnostic
extern bool     sxe_alloc_assert_on_enomem;    // Set to true to assert when out of memory
extern uint64_t sxe_allocations;               // Tracks the number of open allocations in debug builds (always 0 in release)

/* Indirect memory operations. These may be copied and/or overridden by users of the library. Defaults are sxe_*_default
 */
extern void  (*sxe_free)(    void *,         const char *, int);
extern void *(*sxe_malloc)(  size_t,         const char *, int);
extern void *(*sxe_memalign)(size_t, size_t, const char *, int);
extern void *(*sxe_realloc)( void *, size_t, const char *, int);

/* Allocate and zero out an array (calloc)
 */
static inline __attribute__((malloc)) void *
sxe_calloc(size_t num, size_t size, const char *file, int line)
{
    size_t total;
    bool   error;

#if SIZE_MAX == ULONG_MAX    // If size_t is an unsigned long (e.g. Linux)
    error = __builtin_umull_overflow(num, size, &total);
#else    // Assume its an unsigned long long (e.g. Windows)
    error = __builtin_umulll_overflow(num, size, &total);
#endif

    if (error) {
        errno = EOVERFLOW;
        return NULL;
    }

    char *result = (*sxe_malloc)(total, file, line);

    if (result)
        memset(result, '\0', total);

    return result;
}

/* Duplicate a string (strdup)
 */
static inline __attribute__((malloc)) char *
sxe_strdup(const char *string, const char *file, int line)
{
    size_t size = strlen(string) + 1;
    char  *result = (*sxe_malloc)(size, file, line);

    if (result)
        memcpy(result, string, size);

    return result;
}

/* Wrappers that look like their standard namesakes but pass file and line for diagnostics
 */
#define sxe_calloc(num, size)     sxe_calloc((num), (size),     SXE_FILE, __LINE__)
#define sxe_free(memory)          sxe_free((memory),            SXE_FILE, __LINE__)
#define sxe_malloc(size)          sxe_malloc((size),            SXE_FILE, __LINE__)
#define sxe_memalign(align, size) sxe_memalign((align), (size), SXE_FILE, __LINE__)
#define sxe_realloc(memory, size) sxe_realloc((memory), (size), SXE_FILE, __LINE__)
#define sxe_strdup(string)        sxe_strdup((string),          SXE_FILE, __LINE__)

#include "sxe-alloc-proto.h"    // Get the prototypes for the default functions

#endif
