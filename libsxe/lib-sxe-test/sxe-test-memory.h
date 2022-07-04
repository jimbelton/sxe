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
 
/* This file contains includable code and should only be used in test programs.
 */
#ifndef __SXE_TEST_MEMORY__
#define __SXE_TEST_MEMORY__ 1

#ifdef __SXE_MOCK_H__
#   ifndef SXE_MOCK_NO_CALLOC
#       error "sxe-test-memory.h is incompatible with mock.h; define SXE_MOCK_NO_CALLOC to mix them"
#   endif
#endif
 
#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <time.h>

#pragma GCC diagnostic ignored "-Waggregate-return"    // Shut gcc up about mallinfo returning a struct

static bool test_memory_initialized = false;

static __attribute__ ((unused)) size_t
test_memory(void)
{
    time_t time0 = 0;
    
    if (!test_memory_initialized) {
        localtime(&time0);                 // Preallocate memory allocated for timezone
        test_memory_initialized = true;
    }

    struct mallinfo info = mallinfo();
    return info.uordblks;
}

#endif
