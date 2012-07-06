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

#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>

#if !defined(WINDOWS_NT) && !defined(__APPLE__) && !defined(__FreeBSD__)
#define  _GNU_SOURCE /* Required for _IO_cookie_io_functions_t  */
#include <libio.h>   /* Required by __USE_GNU <stdio.h>         */
#define  __USE_GNU   /* Required for vasprintf on Linux         */
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "tap.h"

static int
test_vasprintf(char ** str_ptr, const char * fmt, ...)
{
    va_list args;
    int     ret;

    va_start(args, fmt);
    ret = vasprintf(str_ptr, fmt, args);
    va_end(args);

    return ret;
}

int
main(void)
{
    char         * str;
    struct timeval timeval_first;
    struct timeval timeval_next;
    unsigned long  diff;

    plan_tests(4);

    ok(test_vasprintf(&str, "hello, %s.", "world") >= 0,               "vasprintf succeeded: %s", strerror(errno));
    is_eq(str,                                        "hello, world.", "vasprintf formatted 'hello, world.' into the buffer");

    ok(gettimeofday(&timeval_first, NULL)          >= 0,               "gettimeofday got the first time: %s", strerror(errno));

    for (;;) {
        if (gettimeofday(&timeval_next, NULL) < 0) {
            fail("gettimeofday failed the next time: %s", strerror(errno));
            return exit_status();
        }

        if ((timeval_first.tv_sec != timeval_next.tv_sec) || (timeval_first.tv_usec != timeval_next.tv_usec)) {
            break;
        }
    }

    diff = (timeval_next.tv_sec - timeval_first.tv_sec) * 1000000 + timeval_next.tv_usec - timeval_first.tv_usec;
    ok(diff <= 50000, "gettimeofday: Clock period %ld is less than 0.05 seconds", diff);
    return exit_status();
}
