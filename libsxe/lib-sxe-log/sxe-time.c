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
#include "sxe-time.h"

#define          SXE_TIME_DIGITS_IN_FRACTION    9

const unsigned   SXE_TIMESTAMP_LENGTH_MAXIMUM = SXE_TIMESTAMP_LENGTH(0) + SXE_TIME_DIGITS_IN_FRACTION;
const unsigned   SXE_TIME_POWERS_OF_TEN[10]   = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

/* Since these functions will be called from the log package, they should not themselves log */

SXE_TIME
sxe_time_get(void)
{
    struct timeval tv;

    assert(gettimeofday(&tv, NULL) >= 0);
    return ((SXE_TIME)tv.tv_sec << SXE_TIME_BITS_IN_FRACTION) + (SXE_TIME)tv.tv_usec * (1ULL << 32) / 1000000;
}

double
sxe_get_time_in_seconds(void)
{
    struct timeval tv;

    assert(gettimeofday(&tv, NULL) >= 0);
    return (double)tv.tv_sec + 1.e-6 * (double)tv.tv_usec;
}

char *
sxe_time_to_string(SXE_TIME sxe_time, char * buffer, unsigned length)
{
    time_t    unix_time;
    struct tm broken_time;
    unsigned  index;

    unix_time = (time_t)(sxe_time >> SXE_TIME_BITS_IN_FRACTION);

    assert(length >= SXE_TIMESTAMP_LENGTH(0));
    assert(gmtime_r(&unix_time, &broken_time) != NULL);
    assert(strftime(buffer, length, "%Y%m%d%H%M%S", &broken_time) != 0);

    if (length > SXE_TIMESTAMP_LENGTH(0)) {
        sxe_time                            = (SXE_TIME_FRACTION)sxe_time;
        buffer[SXE_TIMESTAMP_LENGTH(0) - 2] = '.';

        if (length >= SXE_TIMESTAMP_LENGTH_MAXIMUM) {
            length = SXE_TIMESTAMP_LENGTH_MAXIMUM;
        }
        else {
            sxe_time /= SXE_TIME_POWERS_OF_TEN[SXE_TIMESTAMP_LENGTH_MAXIMUM - length];
        }

        buffer[length - 1] = '\0';

        for (index = length - 1; index > SXE_TIMESTAMP_LENGTH(0) - 1; index--) {
            buffer[index - 1] = sxe_time % 10 + '0';
            sxe_time         /= 10;
        }
    }

    return buffer;
}
