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

#include <stdint.h>

#include "sxe-util.h"

/**
 * Base 2 logarithm of a uint64_t as an unsinged int
 *
 * @note This implementation is non-portable. See sxe-unsigned for a portable version for unsigned ints
 */
unsigned
sxe_uint64_log2(uint64_t value)
{
    return 63 - __builtin_clzll(value);
}

uint64_t
sxe_uint64_align(uint64_t value, uint64_t multiple)
{
    uint64_t mod;
     
    if (multiple & (multiple - 1))
        return (mod = value % multiple) ? value - mod + multiple : value;

    return (mod = value & (multiple - 1)) ? value - mod + multiple : value;
}
