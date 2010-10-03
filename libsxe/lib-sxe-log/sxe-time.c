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

#include <assert.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "mock.h"
#include "sxe-log.h"

/* Since these functions will be called from the log package, they should not themselves log */

double
sxe_get_time_in_seconds(void)
{
    struct timeval tv;

    assert(gettimeofday(&tv, NULL) >= 0);
    return (double)tv.tv_sec + 1.e-6 * (double)tv.tv_usec;
}

char *
sxe_time_to_string(char * buffer, unsigned length, double double_time)
{
    time_t    unix_time;
    struct tm broken_time;

    unix_time = (time_t)double_time;

    assert(gmtime_r(&unix_time, &broken_time) != NULL);
    assert(strftime(buffer, length, "%Y%m%d%H%M%S", &broken_time) != 0);
    assert(snprintf(&buffer[14], length - 14, ".%03u", (unsigned)((double_time - (unsigned)double_time) * 1000.0)) == 4);
    return buffer;
}
