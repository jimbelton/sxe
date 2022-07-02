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

#include <tap.h>

#include "sxe-alloc.h"

int
main(void)
{
    char    *memory, *newmem;
    uint64_t init_allocations;
    
    tap_plan(11, 0, NULL);
    sxe_alloc_diagnostics = true;               // Enable sxe-alloc diagnostics
    init_allocations      = sxe_allocations;

    tap_test_case_name("Call each indirect function");
    ok(memory = sxe_malloc(16),         "Allocated 16 bytes");
    sxe_free(memory);
    
    ok(memory = sxe_memalign(16, 16),   "Allocated 16 bytes aligned on a 16 byte boundary");
    is((uintptr_t)memory % 16, 0,       "Memory is correctly aligned");
    is(sxe_realloc(memory, 0), NULL,    "Realloc with size 0 is a free");
    ok(memory = sxe_realloc(NULL, 16),  "Realloc with NULL pointer is a malloc");
    ok(newmem = sxe_realloc(memory, 8), "Realloc can shrink in place");
    sxe_free(newmem);

    tap_test_case_name("Call each inline function");
    ok(memory = sxe_strdup("hello, world"), "Duplicated a string");
    is_eq(memory, "hello, world",           "Contents are correct");
    sxe_free(memory);
    ok(memory = sxe_calloc(1, 8),           "Allocated 8 bytes of zeroed memory");
    is(*(uintptr_t *)memory, 0,             "It's zeroed");
    sxe_free(memory);

    ok(init_allocations == sxe_allocations,
       "Not all memory allocated during the test was freed. Find the leak with sxe-alloc-analyze");
    return exit_status();
}
