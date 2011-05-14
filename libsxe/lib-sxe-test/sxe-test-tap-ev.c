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
#include "sxe-time.h"
#include "sxe-util.h"
#include "tap.h"

#ifdef gettimeofday
#error "gettimeofday should not be mocked here"
#endif

static SXE_TIME
local_get_time(void)
{
    struct timeval tv;

    SXEA11(gettimeofday(&tv, NULL) >= 0, "gettimeofday failed: %s", strerror(errno));
    return SXE_TIME_FROM_TIMEVAL(&tv);
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
    SXE_TIME deadline;
    SXE_TIME current;
    SXE_TIME wait_time;

    wait_time = SXE_TIME_FROM_DOUBLE_SECONDS(seconds);
    event     = tap_ev_queue_shift(queue);

    /* If the tap event queue was not empty, return the event.
     */
    if (event != NULL) {
        return event;
    }

    deadline = local_get_time() + wait_time;

    for (;;) {
        test_ev_loop_wait(SXE_TIME_TO_DOUBLE_SECONDS(wait_time));
        event = tap_ev_queue_shift(queue);

        if (event != NULL) {
            return event;
        }

        current = local_get_time();

        if (current >= deadline) {
            return NULL;
        }

        wait_time = deadline - current;
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

const char *
test_tap_ev_queue_identifier_wait(tap_ev_queue queue, ev_tstamp seconds, tap_ev * ev_ptr)
{
    tap_ev event;

    if (ev_ptr == NULL) {
        ev_ptr = &event;
    }

    *ev_ptr = test_tap_ev_queue_shift_wait(queue, seconds);

    return tap_ev_identifier(*ev_ptr);
}

/**
 * Wait for (possibly fragmented) data on a SXE on a specific TAP queue
 *
 * @param queue           TAP event queue
 * @param seconds         Seconds to wait for
 * @param ev_ptr          Pointer to a tap_ev event
 * @param this            Pointer to a SXE to check as the source of the read events or NULL to allow reads from multiple SXEs
 * @param read_event_name Expected read event identifier (usually "read_event"); event must have "this", "buf" and "used" args
 * @param buffer          Pointer to a buffer to store the data
 * @param expected_length Expect amount of data to be read
 * @param who             Textual description of SXE for diagnostics
 */
void
test_ev_queue_wait_read(tap_ev_queue queue, ev_tstamp seconds, tap_ev * ev_ptr, void * this, const char * read_event_name,
                        char * buffer, unsigned expected_length, const char * who)
{
    unsigned used = 0;
    unsigned i;

    for (i = 0; i < expected_length; i++) {
        if (strcmp(test_tap_ev_queue_identifier_wait(queue, seconds, ev_ptr), read_event_name) != 0) {
            fail("%s expected read event '%s', got event '%s' (already read %u fragments, %u bytes of %u expected)", who,
                 read_event_name, (const char *)tap_ev_identifier(*ev_ptr), i, used, expected_length);
            goto SXE_ERROR_OUT;
        }

        if (this != NULL && this != tap_ev_arg(*ev_ptr, "this")) {
            fail("Expected a read event on SXE %p, got it on %p (%s)", this, tap_ev_arg(*ev_ptr, "this"), who);
            goto SXE_ERROR_OUT;
        }

        if (used + SXE_CAST(unsigned, tap_ev_arg(*ev_ptr, "used")) > expected_length) {
            fail("Expected to read %u, read %u", expected_length, used + SXE_CAST(unsigned, tap_ev_arg(*ev_ptr, "used")));
            memcpy(&buffer[used], tap_ev_arg(*ev_ptr, "buf"), expected_length - used);
            return;
        }

        memcpy(&buffer[used], tap_ev_arg(*ev_ptr, "buf"), SXE_CAST(unsigned, tap_ev_arg(*ev_ptr, "used")));
        used += SXE_CAST(unsigned, tap_ev_arg(*ev_ptr, "used"));

        if (used == expected_length) {
            pass("Read %u bytes on %s", used, who);
            return;
        }
    }

    SXEA12(used == expected_length, "test_ev_wait_read: Must have read %u bytes, but only read %u", expected_length, used);

SXE_ERROR_OUT:
    if (used == 0) {
        strncpy(buffer, "NO DATA READ", expected_length);
    }
    else if (used <  expected_length) {
        buffer[used] = '\0';
    }
}

/**
 * Wait for (possibly fragmented) data on a SXE on the default TAP queue
 *
 * @param seconds         Seconds to wait for
 * @param ev_ptr          Pointer to a tap_ev event
 * @param this            Pointer to a SXE to check as the source of the read events or NULL to allow reads from multiple SXEs
 * @param read_event_name Expected read event identifier (usually "read_event"); event must have "this", "buf" and "used" args
 * @param buffer          Pointer to a buffer to store the data
 * @param expected_length Expect amount of data to be read
 * @param who             Textual description of SXE for diagnostics
 */
void
test_ev_wait_read(ev_tstamp seconds, tap_ev * ev_ptr, void * this, const char * read_event_name, char * buffer,
                  unsigned expected_length, const char * who)
{
    test_ev_queue_wait_read(tap_ev_queue_get_default(), seconds, ev_ptr, this, read_event_name, buffer, expected_length, who);
        }
