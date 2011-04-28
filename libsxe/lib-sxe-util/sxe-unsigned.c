/* Copyright 2010 Sophos Limited. All rights reserved. Sophos is a registered
 * trademark of Sophos Limited.
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

#include "sxe-util.h"

/* Log2 table for all byte values. Each is stored in a nibble to reduce cache thrash.
 */
static unsigned sxe_log2_table[] = {
    0x22221100, 0x33333333, 0x44444444, 0x44444444, 0x55555555, 0x55555555, 0x55555555, 0x55555555,
    0x66666666, 0x66666666, 0x66666666, 0x66666666, 0x66666666, 0x66666666, 0x66666666, 0x66666666,
    0x77777777, 0x77777777, 0x77777777, 0x77777777, 0x77777777, 0x77777777, 0x77777777, 0x77777777,
    0x77777777, 0x77777777, 0x77777777, 0x77777777, 0x77777777, 0x77777777, 0x77777777, 0x77777777
};

static unsigned sxe_mask_table[] =
{
    0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F, 0x00000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF, 0x00001FFF, 0x000003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x0003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF, 0x03FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
};

unsigned
sxe_unsigned_log2(unsigned number)
{
    unsigned shift = 0;

    if (number > 0xFFFF) {
        if (number > 0xFFFFFF) {
            shift = 24;
        }
        else {
            shift = 16;
        }
    }
    else if (number > 0xFF) {
        shift = 8;
    }
    else {
        SXEL93("number %u, number >> 3 = %u, sxe_log2_table[number >> 3] = %08x", number, number >> 3, sxe_log2_table[number >> 3]);
        SXEL93("number & 0x7 = %u, (number & 0xF) * 4 = %u, sxe_log2_table[number >> 3] >> ((number & 0x7) * 4) = %08x", number & 0x7, (number & 0xF) * 4, sxe_log2_table[number >> 3] >> ((number & 0xF) * 4));
        return (sxe_log2_table[number >> 3] >> ((number & 0x7) * 4)) & 0xF;
    }

    number >>= shift;
    return ((sxe_log2_table[number >> 3] >> ((number & 0x7) * 4)) & 0xF) + shift;
}

unsigned
sxe_unsigned_mask(unsigned number)
{
    return sxe_mask_table[sxe_unsigned_log2(number)];
}
