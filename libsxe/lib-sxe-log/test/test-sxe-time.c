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

#include <time.h>
#include "sxe-log.h"
#include "tap.h"

int
main(void)
{
    time_t expected;
    time_t actual;
    char   buffer[SXE_TIMESTAMP_SIZE];

    plan_tests(3);
    time(&expected);
    actual = (time_t)sxe_get_time_in_seconds();
    ok((actual == expected) || (actual == expected + 1),
       "Actual time (%u) is as expected (%u)", (unsigned)actual, (unsigned)expected);
    is_eq(sxe_time_to_string(buffer, sizeof(buffer), 0.0), "19700101000000.000", "Formatted double time as expected");
    sxe_time_to_string(buffer, sizeof(buffer), (double)actual);
    is_eq(&buffer[14], ".000", "Zeroed fractional part formatted as expected");
    return exit_status();
}
