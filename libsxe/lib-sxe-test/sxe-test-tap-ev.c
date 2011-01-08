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

#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "ev.h"
#include "sxe-log.h"
#include "sxe-test.h"
#include "tap.h"

#ifdef gettimeofday
#error "gettimeofday should not be mocked here"
#endif

static double
local_get_time_in_seconds(void)
{
    struct timeval tv;

    SXEA11(gettimeofday(&tv, NULL) >= 0, "gettimeofday failed: %s", strerror(errno));
    return (double)tv.tv_sec + 1.e-6 * (double)tv.tv_usec;
}

unsigned
test_tap_ev_length_nowait(void)
{
    test_process_all_libev_events();
    return tap_ev_length();
}

tap_ev
test_tap_ev_queue_shift_wait(tap_ev_queue queue, ev_tstamp seconds)
{
    tap_ev event;
    double deadline;
    double current;

    event = tap_ev_queue_shift(queue);

    /* If the tap event queue was not empty, return the event.
     */
    if (event != NULL) {
        return event;
    }

    deadline = local_get_time_in_seconds() + seconds;

    for (;;) {
        test_ev_loop_wait(seconds);
        event = tap_ev_queue_shift(queue);

        if (event != NULL) {
            return event;
        }

        current = local_get_time_in_seconds();

        if (current >= deadline) {
            return NULL;
        }

        seconds = deadline - current;
    }
}

tap_ev
test_tap_ev_shift_wait(ev_tstamp seconds)
{
    return test_tap_ev_queue_shift_wait(tap_ev_queue_get_default(), seconds);
}

const char *
test_tap_ev_identifier_wait(ev_tstamp seconds, tap_ev * ev_ptr)
{
    tap_ev event;

    return tap_ev_identifier(*(ev_ptr == NULL ? &event : ev_ptr) = test_tap_ev_shift_wait(seconds));
}
