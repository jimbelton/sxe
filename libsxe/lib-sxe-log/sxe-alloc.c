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

/* sxe-alloc allows caller defined memory functions to be injected into libsxe or any other library that uses sxe-alloc.
 */

#include <malloc.h>    // CONVENTION EXCLUSION: glibc allocation calls are allowed, since this is the interface to them

#include "sxe-alloc.h"
#include "sxe-atomic.h"
#include "sxe-log.h"

bool     sxe_alloc_diagnostics      = false;
bool     sxe_alloc_assert_on_enomem = false;    // By default, return NULL on failure to allocate memory
uint64_t sxe_allocations            = 0;        // Number of open allocations in debug builds only (must be atomically updated)

#define SXE_ALLOC_LOG(...) do { if (sxe_alloc_diagnostics) SXEL5(__VA_ARGS__); } while (0)

void
sxe_free_default(void *ptr, const char *file, int line)
{
    if (ptr) {
        SXE_ALLOC_LOG("%s: %d: sxe_free(%p)", file, line, ptr);
        SXEA6(!(((long)ptr) & 7), "ungranular free(%p)", ptr);
#if SXE_DEBUG
        sxe_atomic_sub64(&sxe_allocations, 1);
#endif
    }
#if SXE_DEBUG
    else if (sxe_alloc_diagnostics)
        SXEL6("%s: %d: sxe_free((nil))", file, line);
#endif

    free(ptr);    // CONVENTION EXCLUSION: glibc allocation calls are allowed, since this is the interface to them
}

__attribute__((malloc)) void *
sxe_malloc_default(size_t size, const char *file, int line)
{
#if SXE_DEBUG
    sxe_atomic_add64(&sxe_allocations, 1);
#endif

    void *result = malloc(size);    // CONVENTION EXCLUSION: glibc allocation calls are allowed, since this is the interface to them

    SXEA1(!sxe_alloc_assert_on_enomem || result, ": failed to allocate %zu bytes of memory", size);
    SXE_ALLOC_LOG("%s: %d: %p = sxe_malloc(%zu)", file, line, result, size);
    return result;
}

__attribute__((malloc)) void *
sxe_memalign_default(size_t align, size_t size, const char *file, int line)
{
#if SXE_DEBUG
    sxe_atomic_add64(&sxe_allocations, 1);
#endif

    char *result = memalign(align, size);    // CONVENTION EXCLUSION: glibc allocation calls are allowed, since this is the interface to them

    if (result == NULL)
        SXEA1(!sxe_alloc_assert_on_enomem, ": failed to allocate %zu bytes of %zu aligned memory", size, align);    /* COVERAGE EXCLUSION: Out of memory condition */

    SXE_ALLOC_LOG("%s: %d: %p = sxe_memalign(%zu,%zu)", file, line, result, align, size);
    return result;
}

/*-
 * Special case for realloc() since it can behave like malloc() or free() !
 */
void *
sxe_realloc_default(void *memory, size_t size, const char *file, int line)
{
#if SXE_DEBUG
    if (memory == NULL)
        sxe_atomic_add64(&sxe_allocations, 1);
    else if (size == 0)
        sxe_atomic_sub64(&sxe_allocations, 1);
#endif

    void *result = realloc(memory, size);    // CONVENTION EXCLUSION: glibc allocation calls are allowed, since this is the interface to them

    if (result == NULL && (size || memory == NULL))
        SXEA1(!sxe_alloc_assert_on_enomem, ": failed to reallocate object to %zu bytes", size);    /* COVERAGE EXCLUSION: Out of memory condition */

    SXE_ALLOC_LOG("%s: %d: %p = sxe_realloc(%p, %zu)", file, line, result, memory, size);
    return result;
}

/* Default to the implementations in this file
 */
void (*sxe_free)(    void *,         const char *, int) = sxe_free_default;
void *(*sxe_malloc)(  size_t,         const char *, int) = sxe_malloc_default;
void *(*sxe_realloc)( void *, size_t, const char *, int) = sxe_realloc_default;
void *(*sxe_memalign)(size_t, size_t, const char *, int) = sxe_memalign_default;
