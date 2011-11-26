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

#include "sxe.h"
#include "sxe-test.h"
#include "sxe-time.h"

static SXE_TIME
local_get_time(void)
{
    struct timeval tv;

    SXEA1(gettimeofday(&tv, NULL) >= 0, "gettimeofday failed: %s", strerror(errno));
    return SXE_TIME_FROM_TIMEVAL(&tv);
}

#define AT_TIMEOUT_THEN_TIMEOUT 0
#define AT_TIMEOUT_THEN_ASSERT  1

#define \
    static /* stupid: trick genxface.pl into *not* copying these into lib-sxe-proto.h */

static tap_ev
test_tap_ev_queue_shift_wait_or_what(tap_ev_queue queue, ev_tstamp seconds, int at_timeout)
{
    tap_ev event;
    SXE_TIME deadline;
    SXE_TIME current;
    SXE_TIME wait_time;

    wait_time = SXE_TIME_FROM_DOUBLE_SECONDS(seconds);
    deadline  = local_get_time() + wait_time;

    for (;;) {
        ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK); /* run deferred events from before the loop or last loop pass */
        event = tap_ev_queue_shift(queue);
        if (event != NULL) {
            return event;
        }

        if (sxe_get_deferred_count() == 0) {
            test_ev_loop_wait(SXE_TIME_TO_DOUBLE_SECONDS(wait_time));
            event = tap_ev_queue_shift(queue);
            if (event != NULL) {
                return event;
            }
        }

        current = local_get_time();

        if (current >= deadline) {
            if (AT_TIMEOUT_THEN_TIMEOUT == at_timeout) {
                return NULL;
            }

            SXEA1(current < deadline, "test_tap_ev_queue_shift_wait(): Timeout waiting for event");
        }

        wait_time = deadline - current;
    }
}

static tap_ev
test_tap_ev_queue_shift_wait_or_timeout(tap_ev_queue queue, ev_tstamp seconds)
{
    return test_tap_ev_queue_shift_wait_or_what(queue, seconds, AT_TIMEOUT_THEN_TIMEOUT);
}

static tap_ev
test_tap_ev_queue_shift_wait_or_assert(tap_ev_queue queue, ev_tstamp seconds)
{
    return test_tap_ev_queue_shift_wait_or_what(queue, seconds, AT_TIMEOUT_THEN_ASSERT);
}

static tap_ev
test_tap_ev_queue_shift_wait(tap_ev_queue queue, ev_tstamp seconds)
{
    return test_tap_ev_queue_shift_wait_or_what(queue, seconds, AT_TIMEOUT_THEN_ASSERT);
}

static tap_ev
test_tap_ev_shift_wait(ev_tstamp seconds) /* _or_assert */
{
    return test_tap_ev_queue_shift_wait(tap_ev_queue_get_default(), seconds);
}

static tap_ev
test_tap_ev_shift_wait_or_timeout(ev_tstamp seconds)
{
    return test_tap_ev_queue_shift_wait_or_timeout(tap_ev_queue_get_default(), seconds);
}

static const char *
test_tap_ev_identifier_wait(ev_tstamp seconds, tap_ev * ev_ptr)
{
    tap_ev event;

    return tap_ev_identifier(*(ev_ptr == NULL ? &event : ev_ptr) = test_tap_ev_shift_wait(seconds));
}

static const char *
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
static void
test_ev_queue_wait_read(tap_ev_queue queue, ev_tstamp seconds, tap_ev * ev_ptr, void * this, const char * read_event_name,
                        char * buffer, unsigned expected_length, const char * who)
{
    unsigned used = 0;
    unsigned i;

    for (i = 0; i < expected_length; i++) {
        const void *buf;
        unsigned size;

        if ((*ev_ptr = test_tap_ev_queue_shift_wait_or_timeout(queue, seconds)) == NULL) {
            fail("%s expected read event '%s', timed out after %f seconds (already read %u fragments, %u bytes of %u expected)",
                 who, read_event_name, seconds, i, used, expected_length);
            goto SXE_ERROR_OUT;
        }

        if (strcmp(tap_ev_identifier(*ev_ptr), read_event_name) != 0) {
            fail("%s expected read event '%s', got event '%s' (already read %u fragments, %u bytes of %u expected)", who,
                 read_event_name, (const char *)tap_ev_identifier(*ev_ptr), i, used, expected_length);
            goto SXE_ERROR_OUT;
        }

        if (this != NULL && this != tap_ev_arg(*ev_ptr, "this")) {
            fail("Expected a read event on SXE %p, got it on %p (%s)", this, tap_ev_arg(*ev_ptr, "this"), who);
            goto SXE_ERROR_OUT;
        }

        buf  = tap_ev_arg(*ev_ptr, "buf");
        size = SXE_CAST(unsigned, tap_ev_arg(*ev_ptr, "used"));
        if (used + size > expected_length) {
            fail("Expected to read %u, read %u", expected_length, used + size);
            memcpy(&buffer[used], buf, expected_length - used);
            return;
        }

        memcpy(&buffer[used], buf, size);
        used += size;

        if (used == expected_length) {
            pass("Read %u bytes on %s", used, who);
            return;
        }
    }

    SXEA1(used == expected_length, "test_ev_queue_wait_read: Must have read %u bytes, but only read %u", expected_length, used);

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
static void
test_ev_wait_read(ev_tstamp seconds, tap_ev * ev_ptr, void * this, const char * read_event_name, char * buffer,
                  unsigned expected_length, const char * who)
{
    test_ev_queue_wait_read(tap_ev_queue_get_default(), seconds, ev_ptr, this, read_event_name, buffer, expected_length, who);
}

/* We're expecting a number of events, but we're not sure what order they'll be
 * delivered in.  Pass in a string with space separated event names, and
 * they'll all be consumed, with a 2 second timeout.
 */
static int
test_ev_consume_events(const char * expected, int expected_events)
{
    tap_ev event = NULL;
    int result = 0;
    const char ** event_list = NULL;
    unsigned i;
    unsigned item;
    unsigned event_count = 1;
    unsigned found_event = 0;
    char * expected_event_names = strdup(expected);

    SXEE6("%s(expected=%s, expected_events=%d)", __func__, expected, expected_events);

    /* Get a count of the number of events */
    for (i = 0 ; expected_event_names[i] != '\0' ; ++i) {
        if (expected_event_names[i] == ' ') {
            ++event_count;
        }
    }

    SXEL7("%d events", event_count);

    /* Construct an array of pointers for our events */
    event_list = malloc(event_count * sizeof(char *));
    i = 0;
    for (item = 0 ; item < event_count ; ++item) {
        event_list[item] = &expected_event_names[i];
        SXEL7("event %d = '%s'", item, event_list[item]);
        for (; expected_event_names[i] != '\0' && expected_event_names[i] != ' ' ; ++i) { }
        expected_event_names[i] = '\0';
        ++i;
    }

    /* Tick them off as we get them */
    while (found_event < event_count) {
        event = test_tap_ev_queue_shift_wait_or_what(tap_ev_queue_get_default(), expected_events ? 2 : 0.5, expected_events);
        SXEL7("Looking for event '%s'", (const char *)(tap_ev_identifier(event)));
        if (event == NULL) {
            diag("Expecting an event, but didn't get one.");
            goto SXE_ERROR_OUT;
        }
        for (i = 0 ; i < event_count ; ++i) {
            if ((event_list[i] != NULL) && (strcmp(tap_ev_identifier(event), event_list[i]) == 0)) {
                SXEL7("found event '%s'", event_list[i]);
                event_list[i] = NULL;
                ++found_event;
                break;
            }
        }
        if (i == event_count) {
            diag("Event '%s' was not expected at this time, but found %d of %d events that we were expecting", (const char *)tap_ev_identifier(event), found_event, event_count);
            goto SXE_ERROR_OUT;
        }
        tap_ev_free(event);
        event = NULL;
    }

    result = 1;

SXE_ERROR_OUT:
    if (event != NULL) {
        tap_ev_free(event);
    }
    free(SXE_CAST_NOCONST(void *, event_list));
    free(expected_event_names);
    SXER6("return %d", result);
    return result;
}
