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

#include <string.h>

#include "sha1.h"
#include "sxe-time.h"
#include "tap.h"

char value[] = "the quick brown fox jumped over the lazy dog 01234567890!@#$%^&*";

int
main(void)
{
    SXE_TIME    start_time;
    unsigned    i;
    SOPHOS_SHA1 sha1;

    plan_tests(1);
    start_time = sxe_time_get();

    for (i = 0; sxe_time_get() - start_time < sxe_time_from_unix_time(1); i++) {
        sophos_sha1(value, sizeof(value) - 1, (char *)&sha1);
    }

    ok(i > 0, "Computed %u SHA1's of %u byte messages in 1 second", i, (unsigned)sizeof(value) - 1);
    return exit_status();
}
