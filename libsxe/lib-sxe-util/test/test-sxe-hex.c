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

#include "sxe-util.h"
#include "tap.h"

int
main(void)
{
    plan_tests(4);
    is(sxe_hex_to_unsigned("0",     2), 0,                    "'0':2    -> 0");
    is(sxe_hex_to_unsigned("face",  4), 0xface,               "'face':4 -> 0xface");
    is(sxe_hex_to_unsigned("B00B",  2), 0xb0,                 "'B00B':2 -> 0xb0");
    is(sxe_hex_to_unsigned("XXXX",  4), SXE_UNSIGNED_MAXIMUM, "'XXXX':4 -> 0x%x (SXE_UNSIGNED_MAXIMUM)",
       sxe_hex_to_unsigned("XXXX",  4));
    return exit_status();
}
