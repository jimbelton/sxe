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

#include "ev.h"
#include "sxe-log.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include <stdlib.h>
#include <string.h>

static void
test_event_timeout(struct ev_loop * loop, ev_timer * timer, int events)
{
    SXEE6("test_event_timeout(loop=%p, timer=%p, events=%d) // should never happen in a correctly running test", loop, timer, events);
    SXEL2("test_event_timeout: Test timer expired while waiting for an event");
    SXE_UNUSED_PARAMETER(loop);
    SXE_UNUSED_PARAMETER(timer);
    SXE_UNUSED_PARAMETER(events);

    SXER6("return");
}

void
test_ev_loop_wait(ev_tstamp seconds)
{
    ev_timer timeout_watcher;

    //special case: reduce log noise: SXEE6("test_ev_loop_wait(seconds=%f)", seconds);
    SXEL6("test_ev_loop_wait(%fs){}", seconds);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"    /* beause ev_*init() are broken */
    ev_timer_init(&timeout_watcher, test_event_timeout, seconds, 0.);
#pragma GCC diagnostic pop
    ev_timer_start(ev_default_loop(EVFLAG_AUTO), &timeout_watcher);
    ev_loop       (ev_default_loop(EVFLAG_AUTO), EVLOOP_ONESHOT  );
    ev_timer_stop (ev_default_loop(EVFLAG_AUTO), &timeout_watcher);
    //special case: reduce log noise: SXER6("return");
}

void
test_process_all_libev_events(void)
{
    SXEE6("test_process_all_libev_events()");

    /* TODO: Once we've upgraded libev, there is a better way to do this */

    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    SXER6("return");
}

/* We're expecting a number of events, but we're not sure what order they'll be
 * delivered in.  Pass in a string with space separated event names, and
 * they'll all be consumed, with a 2 second timeout.
 */
int
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
